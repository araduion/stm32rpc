#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>

void spi2_init();
void write_spi2(char * buf, uint16_t len);
void read_spi2(char * buf, uint16_t len);
void spi2_transf(char * rxbuf, uint16_t rxcnt, char *txbuf, uint16_t txcnt);

#endif

