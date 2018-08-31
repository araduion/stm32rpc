#ifndef __DMA_H__
#define __DMA_H__

#include "st/stm32f10x.h"

typedef enum { P2MEM = 0, MEM2P = 1} dma_dir_t;
typedef void (*dma_cb)(void);

void dma_enable(void);
void set_dma_ch_and_int(DMA_Channel_TypeDef *ch, dma_cb cb);
void config_channel(void *per, void *mem, uint16_t size, dma_dir_t dir);
uint32_t get_dma_int_stat(void);
void disable_channel(void);
void dma_mem_inc(bool state);

#endif
