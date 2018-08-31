#include <stdint.h>

#include "st/stm32f10x.h"
#include "common.h"
#include "dma.h"
#include "uart.h"

static DMA_Channel_TypeDef *cr_dma_ch;
static dma_cb global_dma_cb = 0;

void dma_ch_handler(void) __attribute__ ((interrupt("IRQ")));

extern bool multi_thread;

void dma_ch_handler(void)
{
    multi_thread = 0; /* wait busy printf because we are in int */
    if (global_dma_cb) {
        global_dma_cb();
    } else {
        uint32_t sr = DMA1->ISR;
        uart_printf("default dma callback!!!\r\nDMA1->ISR=%x\r\n", sr);
        // try to recover - clear all possible int
        DMA1->IFCR = ((1 << 28) - 1);
    }
    multi_thread = 1;
}

void dma_enable(void)
{
    RCC->AHBENR |=1; /* enable DMA1 clock */
}

/* channel is like DMA1_Channel6 */
void inline set_dma_ch_and_int(DMA_Channel_TypeDef *ch, dma_cb cb)
{
    cr_dma_ch = ch;
    if (cb) {
        global_dma_cb = cb;
    }
}


void config_channel(void *per, void *mem, uint16_t size, dma_dir_t dir)
{
    if (! cr_dma_ch ) {
        uart_printf("call first set_dma_ch_and_int!\r\n");
        return;
    }
    cr_dma_ch->CCR &= ~1;
    /* 8 bits + mem inc + dir 0 = read from per */
    cr_dma_ch->CCR |= 1 << 7 | dir << 4 | 1 << 3 | 1 << 1;
    cr_dma_ch->CMAR = (uint32_t)mem;
    cr_dma_ch->CPAR = (uint32_t)per;
    cr_dma_ch->CNDTR = size;
    cr_dma_ch->CCR |= 1;
}

void disable_channel(void)
{
    cr_dma_ch->CCR &= ~1;
}

