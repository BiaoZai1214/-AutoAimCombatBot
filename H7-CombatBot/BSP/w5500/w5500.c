#include "w5500.h"

#define _W5500_SPI_VDM_OP_          0x00
#define _W5500_SPI_FDM_OP_LEN1_     0x01
#define _W5500_SPI_FDM_OP_LEN2_     0x02
#define _W5500_SPI_FDM_OP_LEN4_     0x03

#if (_WIZCHIP_ == W5500)

uint8_t  WIZCHIP_READ(uint32_t AddrSel)
{
   uint8_t ret;
   uint8_t spi_data[3];

   WIZCHIP_CRITICAL_ENTER();
   WIZCHIP.CS._select();

   AddrSel |= (_W5500_SPI_READ_ | _W5500_SPI_VDM_OP_);

   if(!WIZCHIP.IF.SPI._read_burst || !WIZCHIP.IF.SPI._write_burst)
   {
       WIZCHIP.IF.SPI._write_byte((AddrSel & 0x00FF0000) >> 16);
       WIZCHIP.IF.SPI._write_byte((AddrSel & 0x0000FF00) >>  8);
       WIZCHIP.IF.SPI._write_byte((AddrSel & 0x000000FF) >>  0);
   }
   else
   {
        spi_data[0] = (AddrSel & 0x00FF0000) >> 16;
        spi_data[1] = (AddrSel & 0x0000FF00) >> 8;
        spi_data[2] = (AddrSel & 0x000000FF) >> 0;
        WIZCHIP.IF.SPI._write_burst(spi_data, 3);
   }
   ret = WIZCHIP.IF.SPI._read_byte();

   WIZCHIP.CS._deselect();
   WIZCHIP_CRITICAL_EXIT();
   return ret;
}

void WIZCHIP_WRITE(uint32_t AddrSel, uint8_t wb)
{
   uint8_t spi_data[4];

   WIZCHIP_CRITICAL_ENTER();
   WIZCHIP.CS._select();

   AddrSel |= (_W5500_SPI_WRITE_ | _W5500_SPI_VDM_OP_);

   if(!WIZCHIP.IF.SPI._write_burst)
   {
        WIZCHIP.IF.SPI._write_byte((AddrSel & 0x00FF0000) >> 16);
        WIZCHIP.IF.SPI._write_byte((AddrSel & 0x0000FF00) >>  8);
        WIZCHIP.IF.SPI._write_byte((AddrSel & 0x000000FF) >>  0);
        WIZCHIP.IF.SPI._write_byte(wb);
   }
   else
   {
        spi_data[0] = (AddrSel & 0x00FF0000) >> 16;
        spi_data[1] = (AddrSel & 0x0000FF00) >> 8;
        spi_data[2] = (AddrSel & 0x000000FF) >> 0;
        spi_data[3] = wb;
        WIZCHIP.IF.SPI._write_burst(spi_data, 4);
   }

   WIZCHIP.CS._deselect();
   WIZCHIP_CRITICAL_EXIT();
}

void WIZCHIP_READ_BUF(uint32_t AddrSel, uint8_t* pBuf, uint16_t len)
{
   uint8_t spi_data[3];
   uint16_t i;

   WIZCHIP_CRITICAL_ENTER();
   WIZCHIP.CS._select();

   AddrSel |= (_W5500_SPI_READ_ | _W5500_SPI_VDM_OP_);

   if(!WIZCHIP.IF.SPI._read_burst || !WIZCHIP.IF.SPI._write_burst)
   {
        WIZCHIP.IF.SPI._write_byte((AddrSel & 0x00FF0000) >> 16);
        WIZCHIP.IF.SPI._write_byte((AddrSel & 0x0000FF00) >>  8);
        WIZCHIP.IF.SPI._write_byte((AddrSel & 0x000000FF) >>  0);
        for(i = 0; i < len; i++)
           pBuf[i] = WIZCHIP.IF.SPI._read_byte();
   }
   else
   {
        spi_data[0] = (AddrSel & 0x00FF0000) >> 16;
        spi_data[1] = (AddrSel & 0x0000FF00) >> 8;
        spi_data[2] = (AddrSel & 0x000000FF) >> 0;
        WIZCHIP.IF.SPI._write_burst(spi_data, 3);
        WIZCHIP.IF.SPI._read_burst(pBuf, len);
   }

   WIZCHIP.CS._deselect();
   WIZCHIP_CRITICAL_EXIT();
}

void WIZCHIP_WRITE_BUF(uint32_t AddrSel, uint8_t* pBuf, uint16_t len)
{
   uint8_t spi_data[3];
   uint16_t i;

   WIZCHIP_CRITICAL_ENTER();
   WIZCHIP.CS._select();

   AddrSel |= (_W5500_SPI_WRITE_ | _W5500_SPI_VDM_OP_);

   if(!WIZCHIP.IF.SPI._write_burst)
   {
        WIZCHIP.IF.SPI._write_byte((AddrSel & 0x00FF0000) >> 16);
        WIZCHIP.IF.SPI._write_byte((AddrSel & 0x0000FF00) >>  8);
        WIZCHIP.IF.SPI._write_byte((AddrSel & 0x000000FF) >>  0);
        for(i = 0; i < len; i++)
            WIZCHIP.IF.SPI._write_byte(pBuf[i]);
   }
   else
   {
        spi_data[0] = (AddrSel & 0x00FF0000) >> 16;
        spi_data[1] = (AddrSel & 0x0000FF00) >> 8;
        spi_data[2] = (AddrSel & 0x000000FF) >> 0;
        WIZCHIP.IF.SPI._write_burst(spi_data, 3);
        WIZCHIP.IF.SPI._write_burst(pBuf, len);
   }

   WIZCHIP.CS._deselect();
   WIZCHIP_CRITICAL_EXIT();
}

uint16_t getSn_TX_FSR(uint8_t sn)
{
   uint16_t val=0,val1=0;

   do
   {
      val1 = WIZCHIP_READ(Sn_TX_FSR(sn));
      val1 = (val1 << 8) + WIZCHIP_READ(WIZCHIP_OFFSET_INC(Sn_TX_FSR(sn),1));
      if (val1 != 0)
      {
        val = WIZCHIP_READ(Sn_TX_FSR(sn));
        val = (val << 8) + WIZCHIP_READ(WIZCHIP_OFFSET_INC(Sn_TX_FSR(sn),1));
      }
   }while (val != val1);
   return val;
}

uint16_t getSn_RX_RSR(uint8_t sn)
{
   uint16_t val=0,val1=0;

   do
   {
      val1 = WIZCHIP_READ(Sn_RX_RSR(sn));
      val1 = (val1 << 8) + WIZCHIP_READ(WIZCHIP_OFFSET_INC(Sn_RX_RSR(sn),1));
      if (val1 != 0)
      {
        val = WIZCHIP_READ(Sn_RX_RSR(sn));
        val = (val << 8) + WIZCHIP_READ(WIZCHIP_OFFSET_INC(Sn_RX_RSR(sn),1));
      }
   }while (val != val1);
   return val;
}

void wiz_send_data(uint8_t sn, uint8_t *wizdata, uint16_t len)
{
   uint16_t ptr = 0;
   uint32_t addrsel = 0;

   if(len == 0)  return;
   ptr = getSn_TX_WR(sn);
   addrsel = ((uint32_t)ptr << 8) + (WIZCHIP_TXBUF_BLOCK(sn) << 3);
   WIZCHIP_WRITE_BUF(addrsel,wizdata, len);
   ptr += len;
   setSn_TX_WR(sn,ptr);
}

void wiz_recv_data(uint8_t sn, uint8_t *wizdata, uint16_t len)
{
   uint16_t ptr = 0;
   uint32_t addrsel = 0;

   if(len == 0) return;
   ptr = getSn_RX_RD(sn);
   addrsel = ((uint32_t)ptr << 8) + (WIZCHIP_RXBUF_BLOCK(sn) << 3);
   WIZCHIP_READ_BUF(addrsel, wizdata, len);
   ptr += len;
   setSn_RX_RD(sn,ptr);
}

void wiz_recv_ignore(uint8_t sn, uint16_t len)
{
   uint16_t ptr = 0;

   ptr = getSn_RX_RD(sn);
   ptr += len;
   setSn_RX_RD(sn,ptr);
}

#endif
