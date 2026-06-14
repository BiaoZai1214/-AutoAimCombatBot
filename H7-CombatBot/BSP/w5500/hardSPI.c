#include "hardSPI.h"

void hardSPI_Init(void)
{
    /* SPI4 已在 MX_SPI4_Init() 中完成初始化，此处只需确认CS和RST的GPIO状态 */
    CS_HIGH();
    RST_HIGH();
}

void hardSPI_Start(void)
{
    CS_LOW();
}

void hardSPI_Stop(void)
{
    CS_HIGH();
}
 uint8_t hardSPI_SwapByte(uint8_t byteSend)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&W5500_SPI, &byteSend, &rx, 1, 100);
    return rx;
}
