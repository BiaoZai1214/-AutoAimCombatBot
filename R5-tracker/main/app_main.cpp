/**
 * Color Tracker - ESP-DL HSV 颜色检测 (适配 ESP32-S3)
 * OV3660 → RGB565 → ESP-DL HSV转换 + 掩码 → blob扫描 → PD控制 → I2C → STM32
 *
 * 性能优化:
 *   - ESP-DL HSV查找表替代手动浮点运算
 *   - 静态 ImageTransformer 避免每帧重建
 *   - PSRAM 掩码缓冲复用
 *   - 掩码隔行扫描降低 CPU 开销
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "driver/i2c.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "dl_image.hpp"

using namespace dl;
using namespace dl::image;

static const char *TAG = "tracker";

/* ── 绿色 HSV 阈值 (ESP-DL: H=0~180, S=0~255, V=0~255) ── */
#define HSV_H_MIN   42
#define HSV_H_MAX   75
#define HSV_S_MIN   120
#define HSV_S_MAX   255
#define HSV_V_MIN   70
#define HSV_V_MAX   255
#define MIN_AREA    64

/* ── PD 控制 (增量式, 参照 OpenMV 社区) ──
 * cmd += Kp*error, 误差大→步长大, 靠近自动减速, 天然丝滑
 * GitHub 主流 Kp: 0.05~0.2@160px, 折算 240px 约 0.08~0.3
 */
#define KP              0.15f   /* 比例增益 */
#define KD              0.05f   /* 微分: 抑制过冲 */
#define LOWPASS_ALPHA   0.90f   /* EMA 平滑输出 */
#define DEADBAND_PX     5       /* 像素死区: 5px≈2° @240px */
#define ANGLE_DEADBAND  10      /* 归中死区 1.0° */
#define MAX_PX_ERR      120     /* 像素误差限幅=图像半宽 */
#define PAN_DIR         (-1)   /* 舵机安装方向: -1=反转 */
#define TILT_DIR        (-1)   /* 舵机安装方向: -1=反转 */
#define LOST_RESET_CNT  4

typedef struct { uint8_t id, cx, cy, w, h; } blob_t;
static QueueHandle_t g_blob_q = NULL;

/* ── 静态资源: 避免每帧分配 ── */
static uint8_t        *s_mask     = nullptr;
static int             s_mask_sz  = 0;
static ImageTransformer s_xform;           /* 复用实例 */
static bool            s_xform_init = false;

/* ── 颜色检测: ESP-DL HSV查找表 + 掩码 → blob ── */
static int g_mask_pixels = 0;  /* 调试: 最近一帧的掩码命中像素数 */

static bool detect_color_blob(const uint8_t *buf, int w, int h,
                               int &out_cx, int &out_cy, int &out_bw, int &out_bh)
{
    const int total = w * h;

    /* 惰性分配 PSRAM 掩码缓冲 */
    if (!s_mask || s_mask_sz < total) {
        if (s_mask) free(s_mask);
        s_mask = (uint8_t *)heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_mask) s_mask = (uint8_t *)malloc(total);
        if (!s_mask) { s_mask_sz = 0; return false; }
        s_mask_sz = total;
    }

    /* 源图: 零拷贝, 直接指向 camera fb */
    img_t src = {};
    src.data     = (void *)buf;
    src.width    = w;
    src.height   = h;
    src.pix_type = DL_IMAGE_PIX_TYPE_RGB565LE;

    /* 目标: 掩码 (0/255) */
    img_t dst = {};
    dst.data     = s_mask;
    dst.width    = w;
    dst.height   = h;
    dst.pix_type = DL_IMAGE_PIX_TYPE_HSV_MASK;

    /* 配置 Transformer (仅首次) */
    if (!s_xform_init) {
        s_xform.set_hsv_thr(
            std::array<uint8_t, 3>{HSV_H_MIN, HSV_S_MIN, HSV_V_MIN},
            std::array<uint8_t, 3>{HSV_H_MAX, HSV_S_MAX, HSV_V_MAX}
        );
        s_xform_init = true;
    }
    s_xform.set_src_img(src).set_dst_img(dst);
    s_xform.transform();

    /* ── 扫描掩码: 隔行采样, 降低 CPU 开销 ── */
    int sum_x = 0, sum_y = 0, count = 0;
    int min_x = w, max_x = 0, min_y = h, max_y = 0;

    for (int y = 0; y < h; y += 2) {           /* 隔行 */
        const uint8_t *row = s_mask + y * w;
        for (int x = 0; x < w; x += 2) {       /* 隔列 */
            if (row[x]) {
                sum_x += x;
                sum_y += y;
                count++;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }

    g_mask_pixels = count * 4;  /* 调试: 近似总命中像素数 */

    /* count 计的是 1/4 采样点, 需换算 */
    if (count * 4 < MIN_AREA) return false;

    out_cx = sum_x / count;
    out_cy = sum_y / count;
    out_bw = max_x - min_x;
    out_bh = max_y - min_y;

    return (out_bw >= 6 && out_bh >= 6);
}

/* ── 检测任务 ── */
static void tracker_task(void *arg)
{
    QueueHandle_t frame_q = (QueueHandle_t)arg;
    ESP_LOGI(TAG, "HSV tracker ready");

    blob_t out = {};
    uint8_t no_det = 0;
    uint32_t dbg_frames = 0, dbg_hits = 0, dbg_last = 0, alive_last = 0;

    while (1) {
        camera_fb_t *fb = NULL;
        if (!xQueueReceive(frame_q, &fb, portMAX_DELAY))
            continue;

        int cx, cy, bw, bh;
        bool hit = detect_color_blob(fb->buf, fb->width, fb->height, cx, cy, bw, bh);

        dbg_frames++;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - dbg_last >= 1000) {
            ESP_LOGI(TAG, "[%dfps hit=%lu mask=%d] cx=%d cy=%d %s",
                     dbg_frames * 1000 / (now - dbg_last), dbg_hits,
                     g_mask_pixels,
                     out.cx, out.cy,
                     out.id ? "TRACK" : "LOST");
            dbg_frames = 0;
            dbg_hits = 0;
            dbg_last = now;
        }

        if (hit) {
            dbg_hits++;
            out.id = 2;
            out.cx = cx;
            out.cy = cy;
            out.w = bw;
            out.h = bh;
            xQueueOverwrite(g_blob_q, &out);
            no_det = 0;
        } else {
            no_det++;
            if (no_det >= LOST_RESET_CNT) {
                out.id = 0;
                xQueueOverwrite(g_blob_q, &out);
            }
        }

        if (now - alive_last >= 1000) {
            ESP_LOGI(TAG, "alive %lu", now / 1000);
            alive_last = now;
        }

        esp_camera_fb_return(fb);
    }
}

/* ── I2C 从机任务 (ESP32 ← STM32 主动读取) ──
 * 协议: STM32 Write 0x01 → ESP32 回 5字节 (pan_lo, pan_hi, tilt_lo, tilt_hi, id)
 * pan/tilt = int16 LE, 单位 0.1°; id: 2=锁定 0=丢失
 * 接线: SDA=GPIO47, SCL=GPIO48, 4.7kΩ 上拉到 3.3V
 */
static void i2c_slave_task(void *arg)
{
    QueueHandle_t bq = (QueueHandle_t)arg;

    /* I2C Slave 初始化 */
    i2c_config_t cfg = {};
    cfg.mode = I2C_MODE_SLAVE;
    cfg.sda_io_num = 47;
    cfg.scl_io_num = 48;
    cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.slave.slave_addr = 0x52;        /* ESP32-S3 I2C 从机地址 */
    cfg.slave.maximum_speed = 100000;   /* STM32 用 100kHz 标准模式 */
    i2c_param_config(I2C_NUM_0, &cfg);
    /* RX/TX 各 128 字节 FIFO */
    i2c_driver_install(I2C_NUM_0, cfg.mode, 128, 128, 0);
    ESP_LOGI(TAG, "I2C slave ready @ 0x52 (SDA=47, SCL=48)");

    /* PD 控制器状态 */
    float err_x_prev = 0, err_y_prev = 0;
    float cmd_x = 90, cmd_y = 90;
    float target_x = 90, target_y = 90;       /* 目标角度: blob到来时更新, EMA每3ms平滑 */
    const int cx0 = 120, cy0 = 120;           /* 240x240 中心 */

    blob_t blob = {};
    uint8_t  i2c_tx[5] = {0};                 /* 预填充发送缓冲 */
    uint32_t dbg_tx_cnt = 0, dbg_rx_cnt = 0, dbg_last = 0;

    while (1) {
        /* ── 非阻塞读取 blob, 更新目标角度 ── */
        if (xQueueReceive(bq, &blob, 0) == pdTRUE) {
            if (blob.id) {
                float ex = (float)(blob.cx - cx0);
                float ey = (float)(cy0 - blob.cy);

                /* 每 2 秒打印 blob 坐标 (调试用) */
                {
                    static uint32_t blob_dbg_last = 0;
                    uint32_t now32 = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    if (now32 - blob_dbg_last >= 2000) {
                        blob_dbg_last = now32;
                        ESP_LOGI(TAG, "blob cx=%d cy=%d ex=%.0f ey=%.0f cmd_x=%.1f cmd_y=%.1f",
                                 blob.cx, blob.cy, ex, ey, cmd_x, cmd_y);
                    }
                }

                /* 像素误差限幅 */
                if (ex >  MAX_PX_ERR) ex =  MAX_PX_ERR;
                if (ex < -MAX_PX_ERR) ex = -MAX_PX_ERR;
                if (ey >  MAX_PX_ERR) ey =  MAX_PX_ERR;
                if (ey < -MAX_PX_ERR) ey = -MAX_PX_ERR;

                if (fabsf(ex) < DEADBAND_PX) ex = 0;
                if (fabsf(ey) < DEADBAND_PX) ey = 0;

                /* 增量式 PD: cmd += Kp*error, 误差大→步长大, 靠近自动减速 */
                float dx = KP * ex + KD * (ex - err_x_prev);
                float dy = KP * ey + KD * (ey - err_y_prev);
                err_x_prev = ex;
                err_y_prev = ey;

                /* 单帧增量限幅: 不超过 8° (防突变) */
                if (dx >  80.0f) dx =  80.0f;
                if (dx < -80.0f) dx = -80.0f;
                if (dy >  80.0f) dy =  80.0f;
                if (dy < -80.0f) dy = -80.0f;

                target_x = cmd_x + dx;
                target_y = cmd_y + dy;

                /* 角度限幅 */
                if (target_x < 0.0f)  target_x = 0.0f;
                if (target_x > 180.0f) target_x = 180.0f;
                if (target_y < 0.0f)  target_y = 0.0f;
                if (target_y > 180.0f) target_y = 180.0f;

                /* 归中死区 */
                if (fabsf(target_x - 90.0f) < (float)ANGLE_DEADBAND && fabsf(ex) < DEADBAND_PX) target_x = 90.0f;
                if (fabsf(target_y - 90.0f) < (float)ANGLE_DEADBAND && fabsf(ey) < DEADBAND_PX) target_y = 90.0f;

                i2c_tx[4] = 2;      /* id=2 → 锁定目标 */
            } else {
                i2c_tx[4] = 0;      /* id=0 → 目标丢失 */
            }
        }

        /* ── EMA 平滑: 每 3ms 跑一次, 330Hz 丝滑 ── */
        cmd_x += (1.0f - LOWPASS_ALPHA) * (target_x - cmd_x);
        cmd_y += (1.0f - LOWPASS_ALPHA) * (target_y - cmd_y);

        /* 角度限幅 */
        if (cmd_x < 0.0f)  cmd_x = 0.0f;
        if (cmd_x > 180.0f) cmd_x = 180.0f;
        if (cmd_y < 0.0f)  cmd_y = 0.0f;
        if (cmd_y > 180.0f) cmd_y = 180.0f;

        /* 转换为 STM32 协议: 角度偏移, 0.1° 单位 int16 LE */
        int16_t pan  = (int16_t)((cmd_x - 90.0f) * 10.0f * PAN_DIR);
        int16_t tilt = (int16_t)((cmd_y - 90.0f) * 10.0f * TILT_DIR);

        i2c_tx[0] = (uint8_t)(pan & 0xFF);
        i2c_tx[1] = (uint8_t)((pan >> 8) & 0xFF);
        i2c_tx[2] = (uint8_t)(tilt & 0xFF);
        i2c_tx[3] = (uint8_t)((tilt >> 8) & 0xFF);

        /* ── 处理 STM32 写入的命令字节 ── */
        uint8_t rx[1];
        int rd = i2c_slave_read_buffer(I2C_NUM_0, rx, sizeof(rx), 0);
        if (rd > 0) {
            dbg_rx_cnt++;
        }

        /* ── 随时准备 TX FIFO: STM32 Write 0x01 后 500μs 内会来读 ── */
        i2c_reset_tx_fifo(I2C_NUM_0);
        int wr = i2c_slave_write_buffer(I2C_NUM_0, i2c_tx, sizeof(i2c_tx), 0);
        if (wr > 0) {
            dbg_tx_cnt++;
        }

        /* 每 5 秒输出 I2C 调试 */
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - dbg_last >= 5000) {
            ESP_LOGI(TAG, "I2C slave: tx=%lu rx=%lu pan=%d tilt=%d id=%d",
                     dbg_tx_cnt, dbg_rx_cnt,
                     (int16_t)(i2c_tx[0] | (i2c_tx[1] << 8)),
                     (int16_t)(i2c_tx[2] | (i2c_tx[3] << 8)),
                     i2c_tx[4]);
            dbg_tx_cnt = 0;
            dbg_rx_cnt = 0;
            dbg_last = now;
        }

        vTaskDelay(pdMS_TO_TICKS(3));   /* ~330Hz 更新速率 */
    }
}

/* ── 相机帧循环 ── */
static void cam_task(void *arg)
{
    QueueHandle_t fq = (QueueHandle_t)arg;
    uint32_t dbg_miss = 0, dbg_got = 0, dbg_last = 0;

    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            dbg_got++;
            if (xQueueSend(fq, &fb, 0) != pdTRUE) {
                esp_camera_fb_return(fb);
                dbg_miss++;
            }
        } else {
            dbg_miss++;
        }

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - dbg_last >= 5000) {
            ESP_LOGI(TAG, "cam: got=%lu drop=%lu", dbg_got, dbg_miss);
            dbg_got = 0;
            dbg_miss = 0;
            dbg_last = now;
        }
    }
}

/* ── 入口 ── */
extern "C" void app_main(void)
{
    /* ── 启动诊断: 串口/内存/PSRAM ── */
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  R5 Tracker - ESP32-S3 CAN 启动");
    ESP_LOGI(TAG, "========================================");

    /* 芯片信息 */
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "芯片: %s, rev %d, cores %d",
             chip.model == CHIP_ESP32S3 ? "ESP32-S3" : "UNKNOWN",
             chip.revision, chip.cores);
    /* PSRAM 诊断 */
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t dram_total  = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t dram_free   = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    ESP_LOGI(TAG, "PSRAM: total=%uKB free=%uKB | DRAM: total=%uKB free=%uKB",
             psram_total / 1024, psram_free / 1024,
             dram_total / 1024, dram_free / 1024);

    if (psram_total == 0) {
        ESP_LOGE(TAG, "!!! PSRAM 未检测到! 相机无法工作 !!!");
        ESP_LOGE(TAG, "!!! 请运行: idf.py menuconfig → ESP PSRAM → 启用 !!!");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    /* DMA 内存检查 */
    size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    ESP_LOGI(TAG, "DMA-capable free: %uKB", dma_free / 1024);

    /* ── 队列 ── */
    QueueHandle_t frame_q = xQueueCreate(2, sizeof(camera_fb_t *));
    QueueHandle_t blob_q  = xQueueCreate(1, sizeof(blob_t));
    g_blob_q = blob_q;

    /* ── 相机配置 (ESP32-S3 CAN / ESP-S3-EYE 引脚) ── */
    camera_config_t cfg = {};
    cfg.pin_pwdn     = -1;
    cfg.pin_reset    = -1;
    cfg.pin_xclk     = 15;
    cfg.pin_sccb_sda = 4;
    cfg.pin_sccb_scl = 5;
    cfg.pin_d0       = 11;
    cfg.pin_d1       = 9;
    cfg.pin_d2       = 8;
    cfg.pin_d3       = 10;
    cfg.pin_d4       = 12;
    cfg.pin_d5       = 18;
    cfg.pin_d6       = 17;
    cfg.pin_d7       = 16;
    cfg.pin_pclk     = 13;
    cfg.pin_vsync    = 6;
    cfg.pin_href     = 7;
    cfg.xclk_freq_hz = 15000000;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.pixel_format = PIXFORMAT_RGB565;
    cfg.frame_size   = FRAMESIZE_240X240;
    cfg.fb_count     = 2;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode    = CAMERA_GRAB_LATEST;

    /* 预计帧缓冲大小 */
    int fb_w = 240, fb_h = 240, fb_bpp = 2;
    ESP_LOGI(TAG, "相机配置: %dx%d RGB565 fb=%d xclk=%dMHz",
             fb_w, fb_h, cfg.fb_count, cfg.xclk_freq_hz / 1000000);
    ESP_LOGI(TAG, "预计帧缓冲: %dKB × %d = %dKB",
             fb_w * fb_h * fb_bpp / 1024, cfg.fb_count,
             fb_w * fb_h * fb_bpp * cfg.fb_count / 1024);

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "相机初始化失败: 0x%x", err);
        return;
    }

    /* 传感器信息 */
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        ESP_LOGI(TAG, "传感器 PID=0x%02x VER=0x%02x MIDH=0x%02x MIDL=0x%02x",
                 s->id.PID, s->id.VER, s->id.MIDH, s->id.MIDL);

        /* 根据传感器类型自动翻转 */
        if (s->id.PID == OV3660_PID || s->id.PID == OV2640_PID) {
            s->set_vflip(s, 1);
            ESP_LOGI(TAG, "传感器: VFLIP 已设置");
        } else if (s->id.PID == GC0308_PID) {
            s->set_hmirror(s, 0);
        } else if (s->id.PID == GC032A_PID) {
            s->set_vflip(s, 1);
        }
        if (s->id.PID == OV3660_PID) {
            s->set_saturation(s, -2);
            s->set_whitebal(s, 0);        /* 关自动白平衡, 颜色不漂移 */
            s->set_awb_gain(s, 0);        /* 关 AWB 增益 */
            ESP_LOGI(TAG, "传感器: AWB 已关闭 (颜色稳定)");
        }
    }

    ESP_LOGI(TAG, "相机就绪!");

    xTaskCreatePinnedToCore(cam_task,     "cam",     4096, frame_q, 5, NULL, 1);
    xTaskCreatePinnedToCore(tracker_task, "tracker", 8192, frame_q, 4, NULL, 1);
    xTaskCreatePinnedToCore(i2c_slave_task, "i2c_slv", 4096, blob_q,  6, NULL, 0);
}
