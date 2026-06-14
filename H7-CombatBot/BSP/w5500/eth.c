#include "eth.h"
#include "spi.h"

/* W5500网络参数 */
static uint8_t ip[4]      = {192, 168, 155, 100};
static uint8_t mac[6]     = {110, 120, 130, 140, 150, 160};
static uint8_t submask[4] = {255, 255, 255, 0};
static uint8_t gateway[4] = {192, 168, 155, 1};

static void ETH_Reset(void)
{
    RST_LOW();
    HAL_Delay(10);
    RST_HIGH();
    HAL_Delay(300);
}

void ETH_Init(void)
{
    uint8_t ver;
    int8_t link;
    int8_t i;

    hardSPI_Init();
    user_register_function();
    ETH_Reset();

    /* 重试循环：SPI偶发错误时自动重试 */
    for (i = 0; i < 3; i++) {
        setMR(MR_RST);
        HAL_Delay(50);

        ver = getVERSIONR();
        if (ver == 0x04)
            break;

        ETH_Reset();
    }

    if (ver != 0x04) {
        printf("W5500 ERROR: Version=0x%02X, SPI communication FAILED\r\n", ver);
        return;
    }
    printf("W5500 Version: 0x%02X\r\n", ver);

    uint8_t txsize[_WIZCHIP_SOCK_NUM_];
    uint8_t rxsize[_WIZCHIP_SOCK_NUM_];
    for (i = 0; i < _WIZCHIP_SOCK_NUM_; i++) {
        txsize[i] = 2;
        rxsize[i] = 2;
    }
    if (wizchip_init(txsize, rxsize) != 0) {
        printf("W5500 ERROR: wizchip_init failed!\r\n");
        return;
    }

    setSHAR(mac);
    setSIPR(ip);
    setSUBR(submask);
    setGAR(gateway);
    printf("MAC: %02X-%02X-%02X-%02X-%02X-%02X\r\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("IP:  %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
    printf("Msk: %d.%d.%d.%d\r\n", submask[0], submask[1], submask[2], submask[3]);
    printf("GW:  %d.%d.%d.%d\r\n", gateway[0], gateway[1], gateway[2], gateway[3]);

    link = wizphy_getphylink();
    printf("PHY Link: %s\r\n", (link == PHY_LINK_ON) ? "UP" : "DOWN");

    wiz_PhyConf phyconf;
    phyconf.by    = PHY_CONFBY_SW;
    phyconf.mode  = PHY_MODE_AUTONEGO;
    phyconf.speed = PHY_SPEED_100;
    phyconf.duplex = PHY_DUPLEX_FULL;
    wizphy_setphyconf(&phyconf);

    HAL_Delay(500);

    link = wizphy_getphylink();
    if (link != PHY_LINK_ON) {
        printf("PHY Link: %s (check cable)\r\n", (link == PHY_LINK_ON) ? "UP" : "DOWN");
    }
}

