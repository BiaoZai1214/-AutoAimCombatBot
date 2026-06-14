#include "http_server.h"
#include "socket.h"
#include "eth.h"
#include "w5500.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

#define HTTP_SOCKET   0
#define HTTP_PORT     80

static uint8_t http_listen(uint8_t sn)
{
    uint8_t retry = 100;
    setSn_CR(sn, Sn_CR_LISTEN);
    while (getSn_CR(sn));

    while (retry--)
    {
        if (getSn_SR(sn) == SOCK_LISTEN)
            return 1;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return 0;
}

static int32_t http_recv_timeout(uint8_t sn, uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    while (timeout_ms--)
    {
        uint16_t rxrsr = getSn_RX_RSR(sn);
        if (rxrsr > 0)
            return recv(sn, buf, (len > rxrsr) ? rxrsr : len);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return -1;
}

static void http_send_response(uint8_t sn)
{
    uint8_t ip[4], mac[6], gw[4], sub[4];
    uint8_t link;
    char body[1024];
    char buf[128];
    uint16_t body_len;

    getSIPR(ip);
    getSHAR(mac);
    getGAR(gw);
    getSUBR(sub);
    link = wizphy_getphylink();

    body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<title>W5500 - STM32H750</title>"
        "<style>"
        "body{font-family:sans-serif;margin:40px;background:#1e1e2e;color:#cdd6f4}"
        ".card{background:#313244;border-radius:12px;padding:24px;margin:16px 0}"
        "h1{color:#cba6f7}h2{color:#89b4fa;font-size:18px}"
        ".ok{color:#a6e3a1}.off{color:#f38ba8}"
        "td{padding:4px 16px 4px 0}td:last-child{color:#bac2de}"
        "</style></head><body>"
        "<h1>STM32H750 + W5500</h1>"
        "<div class='card'>"
        "<h2>网络状态</h2>"
        "<table>"
        "<tr><td>IP地址</td><td>%d.%d.%d.%d</td></tr>"
        "<tr><td>子网掩码</td><td>%d.%d.%d.%d</td></tr>"
        "<tr><td>默认网关</td><td>%d.%d.%d.%d</td></tr>"
        "<tr><td>MAC地址</td><td>%02X-%02X-%02X-%02X-%02X-%02X</td></tr>"
        "<tr><td>PHY链路</td><td class='%s'>%s</td></tr>"
        "</table>"
        "</div>"
        "<div class='card'>"
        "<h2>系统信息</h2>"
        "<table>"
        "<tr><td>芯片</td><td>STM32H750XBH6</td></tr>"
        "<tr><td>主频</td><td>400MHz</td></tr>"
        "<tr><td>RTOS</td><td>FreeRTOS v10.3.1</td></tr>"
        "</table></div></body></html>\r\n",
        ip[0], ip[1], ip[2], ip[3],
        sub[0], sub[1], sub[2], sub[3],
        gw[0], gw[1], gw[2], gw[3],
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
        (link == PHY_LINK_ON) ? "ok" : "off",
        (link == PHY_LINK_ON) ? "UP" : "DOWN");

    /* 拼接 HTTP 头 + body */
    uint16_t total = snprintf(buf, sizeof(buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        body_len);

    /* 分两次发送：头 + body，避免大局部变量 */
    send(sn, (uint8_t *)buf, total);
    send(sn, (uint8_t *)body, body_len);
}

void HTTP_Server_Run(void)
{
    uint8_t sn = HTTP_SOCKET;
    uint8_t buf[512];
    int32_t ret;

    ret = socket(sn, Sn_MR_TCP, HTTP_PORT, 0);
    if (ret < 0) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    if (!http_listen(sn)) {
        close(sn);
        vTaskDelay(pdMS_TO_TICKS(1000));
        return;
    }

    while (1) {
        uint8_t status = getSn_SR(sn);

        if (status == SOCK_ESTABLISHED) {
            ret = http_recv_timeout(sn, buf, sizeof(buf), 3000);
            if (ret > 0)
                http_send_response(sn);

            disconnect(sn);
            close(sn);
            return;
        }
        else if (status == SOCK_LISTEN) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        else {
            close(sn);
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }
    }
}
