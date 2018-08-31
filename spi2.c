#include "spi2.h"

#include "st/stm32f10x.h"
#include "arm/irq.h"

#include "timer.h"
#include "uart.h"
#include "common.h"

#include "dma.h"

#include "task.h"

#define current_task (cr_task->task_id)

static int is_spi_init = 0, spi_error = 0;

static char *spi_tx_buf, *spi_rx_buf;

void spi_int_handler(void) __attribute__ ((interrupt("IRQ")));

static uint16_t spi_tx_cnt = 0, spi_rx_cnt = 0;

static unsigned int spi_sr = 0;

static uint8_t spi_task_id;

extern int multi_thread;

void spi_dump_rx(void)
{
    int i;
    for (i = 0; i < spi_rx_cnt; i++) {
        uart_printf("%d ", spi_rx_buf[i]);
    }
    uart_puts("\r\n");
}

static void spi_dma_done(void)
{
    uint32_t sr = DMA1->ISR;
    int is_transferred = false;

    if (sr & 0x80000) {
        uart_printf("DMA error occur %x at write!\r\n", sr);
        DMA1->IFCR |= 0xF0000; /* clear all interrupts */
        disable_channel();
        return;
    }
    if (sr & (1 << 17)) {
        disable_channel();
        /* transfer complete? if put here
         * CS to 1 last two bits are sent
         * with SS = 1! hw bug? */
        DMA1->IFCR |= 0x70000; /* clear ch5 status */
        is_transferred = true;
    }
    if (sr & 0x8000) {
        uart_printf("DMA error occur %x at write!\r\n", sr);
        DMA1->IFCR |= 0xF000; /* clear all interrupts */
        disable_channel();
        return;
    }
    if (sr & (1 << 13)) {
        disable_channel();
        /* transfer complete? if put here
         * CS to 1 last two bits are sent
         * with SS = 1! hw bug? */
        DMA1->IFCR |= 0x7000; /* clear ch4 status */    SPI2->CR1 &=  ~(1 << 6); /* disable SPI*/
        is_transferred = true;
    }
    if (true == is_transferred) task_resume(spi_task_id);
}

void spi2_int_handler(void)
{
    /* on error interrupt */
    multi_thread = 0;
    uart_printf("ERROR: SPI status %08x!!!\r\n", SPI2->SR);
    SPI2->SR &= ~(1 << 4); /* if crc error we can recover */
    if (SPI2->SR & (1 << 7)) {
        uart_printf("SPI busy.. disable it!\r\n");
        SPI2->CR1 &=  ~(1 << 6); /* disable SPI*/
    }
    if (SPI2->SR & (1 << 5)) {
        SPI2->CR1 &=  ~(1 << 6); /* disable SPI*/
        uart_printf("SPI mode fault!\r\n");
    }
    if (SPI2->SR & (1 << 6)) {
        uint16_t aux,sr;
        uart_printf("SPI ovr.. disable it!\r\n");
        aux = SPI2->DR;
        sr = SPI2->SR;
        SPI2->CR1 &=  ~(1 << 6); /* disable SPI*/
    }
    multi_thread = 1;
}

void spi2_init(int two_wires)
{
    /* SSOE hw bug https://my.st.com/public/STe2ecommunities/mcu/Lists/cortex_mx_stm32/Flat.aspx?RootFolder=https%3a%2f%2fmy.st.com%2fpublic%2fSTe2ecommunities%2fmcu%2fLists%2fcortex_mx_stm32%2fSPI%20problem%20with%20hardware%20NSS%20management&FolderCTID=0x01200200770978C69A1141439FE559EB459D7580009C4E14902C3CDE46A77F0FFD06506F5B&currentviews=26515 */

    /* SPI2 pins PB12 SS PB13 SCK PB14 MISO PB15 MOSI*/

    GPIOB->CRH &= ~(1 << 18 | 1 << 22 | 1 << 30);
    //GPIOB->CRH |= 1 << 17 | 1 << 20 | 1 << 21 |1 << 23| 1 << 27 | 1 << 28 | 1 << 29 | 1 << 31;
    GPIOB->CRH |= 1 << 17 | 1 << 20 | 1 << 21 |1 << 23| 1 << 28 | 1 << 29 | 1 << 31;
    GPIOB->ODR |= 1 << 12 | 1 << 14;

    RCC->APB2ENR |= 1 << 12;
    if (two_wires) {
        /* pclk/32 1.5MHz master bidirectional mode half duplex */
        SPI2->CR1 |= 1 << 2 | 1 << 3 | 1 << 4 | 1 << 14 |1 << 15 | 1 << 9 | 1 << 8;
    } else {
        /* full duplex */
        SPI2->CR1 |= 1 << 2 | 1 << 3 | 1 << 4 | 1 << 9 | 1 << 8;
    }
    /* enable interrupts */
    SPI2->CR2 |= 1 | 1 << 1 | 1 << 5; // do we need interrupt on error??

    NVIC_enableIRQ(SPI2_IRQn);
    NVIC_enableIRQ(DMA1_Channel5_IRQn); // write DMA int
    NVIC_enableIRQ(DMA1_Channel4_IRQn); // read DMA int
    is_spi_init = 1;
}

/* DMA channel 5 for SPI2 TX in two wires mode */
void write_spi2(char *buf, uint16_t len)
{
    spi_tx_cnt = len;
    spi_tx_buf = buf;

    SPI2->CR1 &=  ~(1 << 6); /* disable SPI*/
    GPIOB->ODR &= ~(1 << 12); /* CS -> LOW*/
    /* for half duplex...  */
    SPI2->CR1 |=  1 << 14; /* enable SPI output mode */
    spi_task_id = current_task;
    task_suspend(spi_task_id);
    set_dma_ch_and_int(DMA1_Channel5, spi_dma_done); /* Do some DMA magic*/
    config_channel((void*)&SPI2->DR, spi_tx_buf,len,MEM2P);
    SPI2->CR1 |=  1 << 6; /* enable SPI*/
    task_next();
    /*current transfer finish; waked up by DMA int*/
    GPIOB->ODR |= 1 << 12; /* hw bug - set SS to high here because interrupt
                             arrive to early...*/
}

/* DMA channel 4 for SPI2 in two wires mode */
void read_spi2(char * buf, uint16_t len)
{
    SPI2->CR1 &=  ~(1 << 6); /* disable SPI*/
    SPI2->CR1 &= ~(1 << 14); // for bidi mode (half duplex)
    spi_rx_cnt = len;
    spi_rx_buf = buf;
    spi_task_id = 1;
    task_suspend(spi_task_id);
    set_dma_ch_and_int(DMA1_Channel4, spi_dma_done);
    config_channel((void*)&SPI2->DR, spi_rx_buf,len,P2MEM);
    GPIOB->ODR &= ~(1 << 12);
    SPI2->CR1 |=  1 << 6;
    task_next();
}

void spi2_transf(char * rxbuf, uint16_t rxcnt, char *txbuf, uint16_t txcnt)
{
    spi_rx_cnt = rxcnt;
    spi_tx_cnt = txcnt;
    spi_rx_buf = rxbuf;
    spi_tx_buf = txbuf;

    SPI2->CR1 &=  ~(1 << 6); /* disable SPI*/
    spi_task_id = current_task;
    task_suspend(spi_task_id);
    set_dma_ch_and_int(DMA1_Channel4, spi_dma_done);
    config_channel((void*)&SPI2->DR, rxbuf,rxcnt,P2MEM);
    set_dma_ch_and_int(DMA1_Channel5, spi_dma_done); /* Do some DMA magic*/
    config_channel((void*)&SPI2->DR, txbuf,txcnt,MEM2P);
    SPI2->CR1 |=  1 << 6; /* enable SPI*/
    task_next();
    /*current transfer finish; waked up by DMA int*/
}

