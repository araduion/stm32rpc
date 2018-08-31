#include "st/stm32f10x.h"
#include "enc28j60.h"
#include "spi2.h"
#include "uart.h"
#include "net.h"

bool j60_eth_is_init = false;
j60_state_t j60_prev_state = LINK_DOWN;
j60_state_t j60_cr_state = LINK_DOWN;

void j60_write_ctl(uint8_t reg, uint8_t data)
{
    /* 010b - wr ctl, 0x14 phlcon reg,  */
    char cmd[] = {64 | (reg & 0x1f), data}, rx[sizeof(cmd)];
    /* CS to low */
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(rx,2,cmd,2);
    /* CS to high */
    GPIOB->ODR |= 1 << 12;
}

int readdbg = 0;

void set_read_dbg(const char *arg)
{
    readdbg = *arg - '0';
}

/* now works only for control / not mii */
uint8_t j60_read_ctl(uint8_t reg)
{
    char bufrx[3] = {[0 ... 2] = 0},buftx[3] = {[0 ... 2] = 0};
    buftx[0] = reg & 0x1f;
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(bufrx,3,buftx,3);
    GPIOB->ODR |= 1 << 12;

if (readdbg) {
    uart_printf("%s,%d: rx0 %02x rx1 %02x rx2 %02x \r\n", __func__, __LINE__,
                bufrx[0], bufrx[1], bufrx[2]);
}

    return bufrx[1];
}

/* set bit field */
void j60_set_bit(uint8_t reg, uint8_t data)
{    char cmd[] = {128 | (reg & 0x1f), data}, rx[sizeof(cmd)];
    /* CS to low */
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(rx,2,cmd,2);
    /* CS to high */
    GPIOB->ODR |= 1 << 12;
}

/* clear bit field */
void j60_clear_bit(uint8_t reg, uint8_t data)
{    char cmd[] = {160 | (reg & 0x1f), data}, rx[sizeof(cmd)];
    /* CS to low */
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(rx,2,cmd,2);
    /* CS to high */
    GPIOB->ODR |= 1 << 12;
}

static uint8_t last_bank = 0;

void j60_set_bank(uint8_t bank)
{
    uint8_t bits, btoset = 0, btoclear = 0;
    if ((bits = (bank ^ last_bank))) {
        if (bits & 1) {
            if (bank & 1) { /* BSEL0 */
                btoset = 1;
            } else {
                btoclear = 1;
            }
        }
        if (bits & 2) {  /* BSEL1 */
            if (bank & 2) {
                btoset |= 2;
            } else {
                btoclear |= 2;
            }
        }
        last_bank = bank;
        if (btoset) {
            j60_set_bit(ECON1, btoset);
        }
        if (btoclear) {
            j60_clear_bit(ECON1, btoclear);
        }
    }
}

void spi_write_phy_reg(uint8_t reg, uint16_t value)
{
    uint8_t ret;
    /* switch bank 2; disable recv */
    j60_set_bank(2);
    j60_write_ctl(MIREGADR, reg);
    j60_write_ctl(MIWRL, value & 0xFF);
    j60_write_ctl(MIWRH, value >> 8);

    /*wait mii status*/
    j60_set_bank(3);
    do {
        sleep(1);
        ret = j60_read_ctl(MISTAT);
    } while (ret & 1 != 0);
}

#define MIIRD 1

uint16_t spi_read_phy_reg(uint8_t reg)
{
    uint8_t ret;
    uint16_t value;

    j60_set_bank(2);
    j60_write_ctl(MIREGADR, reg);
    j60_write_ctl(MICMD, MIIRD);
    j60_set_bank(3);
    do {
        sleep(1);
        ret = j60_read_ctl(MISTAT);
    } while (ret & 1 != 0);
    j60_set_bank(2);
    j60_write_ctl(MICMD, 0);
    value  = j60_read_ctl(MIRDL);
    value |= j60_read_ctl(MIRDH) << 8;

    return value;
}

void cli_read_phy_reg(const char *arg)
{
    uint8_t reg;
    char *phy_addr = (char*)arg;

    trim(phy_addr);
    reg = atoi16(phy_addr);
    uart_printf("phy(%02x) value %04x\r\n", reg, spi_read_phy_reg(reg));
}

void cli_read_ctrl_reg(const char *arg)
{
    uint8_t bank = arg[0] - '0', adr;
    const char *c = &arg[2];
    trim((char*)c);
    adr = atoi16(c);
    j60_set_bank(bank);
    uart_printf("read reg %02x from bank %02x: %02x\r\n", adr, bank, j60_read_ctl(adr));
}

void cli_write_ctrl_reg(const char *arg)
{
    uint8_t bank = arg[0] - '0', adr, val;
    char *c = (char*)&arg[2],*d = (char*)&arg[2];
    trim((char*)c);
    while (*d != '\0') {
        if (*d == ',') {
            *d = '\0';
            d++;
            break;
        }
        d++;
    }
    adr = atoi16(c);
    val = atoi16(d);
    j60_set_bank(bank);
    uart_printf("write reg %02x from bank %02x: %02x\r\n", adr, bank, val);
    j60_write_ctl(adr,val);
}

void cli_setled(const char *arg)
{
    if (*arg == '0') {
        spi_write_phy_reg(PHLCON, 0x992);
    } else {
        spi_write_phy_reg(PHLCON, 0x882);
    }
}

void cli_reset(const char *arg)
{
    /* 010b - wr ctl, 0x14 phlcon reg,  */
    static char cmd = 255, rx;
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(&rx,1,&cmd,1);
    GPIOB->ODR |= 1 << 12;
    last_bank = 0;
    j60_eth_is_init = false;
}

/* arg is in hex - pattern used to fill eth RAM */
void cli_enc28_selftest(const char *arg)
{
    uint8_t ret;
    uint16_t csum, dma_csum;
    char *pat = (char*)arg; /* set pattern like XX where X hexa digit */
    
    cli_reset(NULL);
    trim(pat);

    /* note form doc: rcv pg 47
     * if rx ptr wrap arround - infinite loop */
    j60_set_bank(3);
    j60_write_ctl(EBSTSD, atoi16(pat));
    j60_write_ctl(EBSTCON, 0x2a); /* config pattern mode enable */
    j60_write_ctl(EBSTCON, 0x2b); /* set start */
    /* start selftest csum calc */
    do {
        sleep(1);
        ret = j60_read_ctl(EBSTCON);
    } while ((ret & 1 ) != 0);
    csum = j60_read_ctl(EBSTCSL);
    csum += j60_read_ctl(EBSTCSH) << 8;
    ret &= ~(1 << 1); /* found that if 'test mode enable' bit is set
                         then DMA get stuck... */
    j60_write_ctl(EBSTCON, ret);
    j60_write_ctl(ECON1, 1 << 4); /* select bank 0 and dma csum */
    /* set dma pos */
    j60_write_ctl(EDMASTL,0);
    j60_write_ctl(EDMASTH,0);
    j60_write_ctl(EDMANDL,0xff);
    j60_write_ctl(EDMANDH,0x1f);
    j60_write_ctl(ECON1,1 << 4 | 1 << 5); /* dma start and set bank 0 */
    do {
        sleep(1);
        ret = j60_read_ctl(ECON1);
    } while ((ret & (1 << 5)) != 0);
    ret = j60_read_ctl(ECON1);
    dma_csum = j60_read_ctl(EDMACSL);
    dma_csum += j60_read_ctl(EDMACSH) << 8;
    j60_write_ctl(ECON1, 1 << BSEL0 | 1 << BSEL1); /* set bank 3 */
    uart_printf("eth mem test selftest csum %04x dma csum test %04x result:%s\r\n", csum ,dma_csum,
            (csum == dma_csum)?"PASS":"FAIL");
    j60_write_ctl(ECON1, 0); /* back to default*/

    j60_eth_is_init = false;
    last_bank = 0;
}

extern bool spi2_tx_no_inc;

void cli_enc28_read(const char *arg)
{
    uint16_t i, adr;
    char rxbuf[] = { [0 ... 80] = 0 }, txbuf[] = { 0x3a, [1 ... 80] = 0 };
    uint32_t *aux, *aux2;

    j60_set_bank(0);
    if (arg != NULL) {
        trim((char*)arg);
        adr = atoi16(arg);
        j60_write_ctl(ERDPTL,adr & 0xFF);
        j60_write_ctl(ERDPTH,(adr >> 8) & 0xFF);
    } else {
        adr = j60_read_ctl(ERDPTL) & 0xFF;
        adr |= j60_read_ctl(ERDPTH) << 8;
    }

    aux = (uint32_t*)&rxbuf[1];
    GPIOB->ODR &= ~(1 << 12);

    spi2_transf(rxbuf,sizeof(rxbuf),txbuf,sizeof(txbuf));

    GPIOB->ODR |= 1 << 12;
    uart_printf("\nmem dump from %04x:\r\n",adr);
    for (i = 0; i < sizeof(rxbuf)/(2*sizeof(uint32_t)); i++,aux+=2) {
        aux2 = aux + 1;
        uart_printf("%04x: %08x %08x\r\n", adr, swap32(*aux), swap32(*aux2));
        adr = (adr + 8) % 8192;
    }
}

void cli_enc28_write(const char *arg)
{
    /* for now rx is 20... is should work with 1 but ... it corrupts
     * stack - ntow become 0 */
    char rxbuf[] = {[0 ... 20] = 0}, txbuf[] = {0x7a, [1 ... 20] = 0};
     /* ff,ef,fe cause entire work become zero!! design with int/uint mixed issue probably*/
    char defBuf[] = { 0xfd, 0x1, 0xef, [3 ... 14] = 0x33, 0xfd };
    uint8_t ntow = sizeof(txbuf), arglen;

    if (arg != NULL) {
        trim((char*)arg);
        arglen = strlen(arg) + 1;
    } else {
        arg = defBuf;
        arglen = 17;
    }

    if (arglen < ntow) ntow = arglen; /* to now overflow txbuf */
    _memcpy(&txbuf[1], (void*)arg, ntow - 1);
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(rxbuf,ntow,txbuf,ntow);
    GPIOB->ODR |= 1 << 12;
    uart_printf("send %d bytes to eth ram\r\n", ntow - 1);
}

void enc28j60_init(const char *arg)
{
    uint16_t phy_reg_val, phid1, phid2;
    int8_t retry_clock = 29, retry_link = 29;
    union {
        uint32_t u32; /* unit identifier */
        uint8_t u8[4];
    } oui;
    uint8_t pn, rev;
    uart_puts("init eth..\r\n");
    do {
        sleep(1); /* yield for other tasks */
        if (0 == (retry_clock % 10)) uart_puts("retry for osc get ready!\r\n");
    } while ((j60_read_ctl(0x1d) & 1) == 0 && (--retry_clock) > 0); /* wait for osc */
    if (retry_clock == 0) {
        uart_puts("eth init fail;oscilator issue?\r\n");
        //return;
        /* in errata of enc28j60 it is known that sometimes this 
         * bit may not be set but we should wait at least 1 ms...*/
    }
    j60_write_ctl(ERDPTL,MEMRX_ST & 0xff);
    j60_write_ctl(ERDPTH,(MEMRX_ST & 0x1f00) >> 8);
    j60_write_ctl(ERXSTL,MEMRX_ST & 0xff);
    j60_write_ctl(ERXSTH,(MEMRX_ST & 0x1f00) >> 8);
    j60_write_ctl(ERXNDL,MEMRX_ND & 0xff);
    j60_write_ctl(ERXNDH,(MEMRX_ND & 0x1f00) >> 8);
    j60_write_ctl(ETXSTL,MEMTX_ST & 0xff);
    j60_write_ctl(ETXSTH,(MEMTX_ST & 0x1f00) >> 8);
#if 0
    j60_write_ctl(ETXNDL,MEMTX_ND & 0xff);
    j60_write_ctl(ETXNDH,(MEMTX_ND & 0x1f00) >> 8);
#endif
    j60_write_ctl(ERXRDPTL,MEMRX_ST & 0xff);
    j60_write_ctl(ERXRDPTH,(MEMRX_ST & 0x1f00) >> 8);
    j60_write_ctl(ERXWRPTL,MEMRX_ST & 0xff);
    j60_write_ctl(ERXWRPTH,(MEMRX_ST & 0x1f00) >> 8);
    phid1 = spi_read_phy_reg(PHID1);
    phid2 = spi_read_phy_reg(PHID2);
    j60_set_bank(3);
    rev = j60_read_ctl(EREVID);
    pn  = (phid2 & 0x1f0) >> 4;
    oui.u32 = ((phid2 >> 10) << 19) | (phid1 << 2);
    uart_printf("read from phy p/n %02x rev B%x\r\n chip id %08x\r\n",
                pn, (rev == 2)?1:(rev == 6)?7:rev, oui.u32);
    do {
        sleep(1); /* yield for other tasks */
        if (0 == (retry_link % 10)) uart_printf("retry for link get ready status %04x!\r\n", phy_reg_val);
        phy_reg_val = spi_read_phy_reg(PHSTAT2);
    } while (((phy_reg_val & (1 << LSTAT)) == 0) && (--retry_link) > 0);
    if (phy_reg_val & (1 << LSTAT)) {
        uart_printf("link up detected (%d).\r\n", retry_link);
        j60_prev_state = LINK_DOWN;
        j60_cr_state = LINK_UP;
    } else {
        j60_prev_state = LINK_DOWN;
        j60_cr_state = LINK_DOWN;
        uart_printf("link down detected (%d)!\r\n", retry_link);
    }
    phy_reg_val = spi_read_phy_reg(PHSTAT2); /* pg 10, pg 22*/

    j60_set_bank(2);
#if 0
    if ((phy_reg_val & (1 << DPXSTAT))) {
#endif
        j60_write_ctl(MACON1, 1 << MARXEN | 1 << RXPAUS | 1 << TXPAUS);
        j60_write_ctl(MACON3, 1 << FULDPX | 1 << FRMLEN | 1 << TXCRCEN );
        j60_write_ctl(MABBIPG, 0x15);
        uart_printf("full duplex led config detected.\r\n");
#if 0
    } else {
        j60_write_ctl(MACON1, 1 << MARXEN);
        j60_write_ctl(MACON3, 1 << FRMLEN | 1 << TXCRCEN );
        j60_write_ctl(MACON4, 1 << DEFER);
        j60_write_ctl(MABBIPG, 0x12);
        spi_write_phy_reg(PHCON2, 1 << 8); /* HDLDIS - PHY Half-Duplex Loopback Disable bit */
        uart_printf("half duplex led config detected.\r\n");
    }
#endif
    /* after reset with no power cycle byts 0 and 1 may change... and wrong behavior;
     * in that case... hw reset module!*/
    j60_write_ctl(MAIPGL, 0x12);
    j60_write_ctl(MAIPGH, 0x0C);
    j60_write_ctl(MAMXFLL, MTU & 0xff);
    j60_write_ctl(MAMXFLH, (MTU & 0xff00) >> 8);
    j60_set_bank(3);
    j60_write_ctl(MAADR6, oui.u8[0]);
    j60_write_ctl(MAADR5, oui.u8[1]);
    j60_write_ctl(MAADR4, oui.u8[2]);
    j60_write_ctl(MAADR3, 0xa3);
    j60_write_ctl(MAADR2, 0x04);
    j60_write_ctl(MAADR1, 0x0);
    uart_printf("set our mac address %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                0,0x4,0xa3,oui.u8[2],oui.u8[1],oui.u8[0]);

    our_mac[5] = oui.u8[0];
    our_mac[4] = oui.u8[1];
    our_mac[3] = oui.u8[2];
    our_mac[2] = 0xa3;
    our_mac[1] = 0x4;
    our_mac[0] = 0;

    /* ipv6 filter*/
    j60_set_bank(1);
    j60_write_ctl(ERXFCON, 0xa2);
    /* activate rx */
    j60_set_bit(ECON1,4);
    j60_eth_is_init = true;
    /* Enable phy interrupt... not specified in interrupt section of datashit */
    spi_write_phy_reg(PHIE, 18);
    /* enable enc28j60 interrupts INT = 7, RX = 6, LINK = 4, TX = 3, TX ERR = 1, RX ERR = 0*/
    j60_write_ctl(EIE, 1 << 7 | 1 << 6 | 1 << 4 | 1 << 3); /* | 1<<1 causing link flapping issues...*/
    spi_read_phy_reg(PHIR); /* this should be clear to detect link status... */
    if (j60_cr_state == LINK_UP) {
        /* mark as ready to send pkts */
        j60_cr_state |= TX_COMPLETE;
    }
}

void cli_enc28_dump_ptr(const char *arg)
{
    register uint16_t aux, b;

    j60_set_bank(0);
    aux = j60_read_ctl(0) | (j60_read_ctl(1) << 8);
    uart_printf("erdpt %04x\r\n", aux);
    aux = j60_read_ctl(2) | (j60_read_ctl(3) << 8);
    uart_printf("ewrpt %04x\r\n", aux);
    aux = j60_read_ctl(4) | (j60_read_ctl(5) << 8);
    uart_printf("etxst %04x\r\n", aux);
    aux = j60_read_ctl(6) | (j60_read_ctl(7) << 8);
    uart_printf("etxnd %04x\r\n", aux);
    aux = j60_read_ctl(8) | (j60_read_ctl(9) << 8);
    uart_printf("erxst %04x\r\n", aux);
    aux = j60_read_ctl(10) | (j60_read_ctl(11) << 8);
    uart_printf("erxnd %04x\r\n", aux);
    aux = j60_read_ctl(12) | (j60_read_ctl(13) << 8);
    uart_printf("erxrdpt %04x\r\n", aux);
    aux = j60_read_ctl(14) | (j60_read_ctl(15) << 8);
    uart_printf("erxwrpt %04x\r\n", aux);
    j60_set_bank(1);
    b = j60_read_ctl(0x18);
    uart_printf("filter %02x\r\n", b);
    b = j60_read_ctl(0x19);
    uart_printf("pkt count %02x\r\n", b);
}

char rx_pkt_buf[] = { [0 ... 1520] = 0 };
char tx_pkt_buf[] = { [0 ... 1520] = 0 };
char ax_pkt_buf[] = { [0 ... 1520] = 0 }; /* with this we send command (read, write, whatever 
                                             and send '\0' for each bye received or receive on
                                             it '\0' for send */
uint16_t j60_rx_ptr = MEMRX_ST;

void cli_enc28_dump_pkts(const char *arg)
{
    uint8_t i, pkt_cnt, tx_buf[] = {0x3a, [ 1 ... sizeof(j60_recv_vect_t) + 1] = 0 };
    uint8_t rx_buf[] = {[0 ... sizeof(j60_recv_vect_t) + 1] = 0};
    uint16_t j;
    j60_recv_vect_t *pkt_vec = (j60_recv_vect_t *)&rx_buf[1];

    j60_set_bank(1);
    pkt_cnt = j60_read_ctl(0x19);
    uart_printf("got %d pkts!\r\n", pkt_cnt);

    j60_set_bank(0);
    for (i = 0; i < pkt_cnt; i++) {
        j60_write_ctl(ERDPTL,j60_rx_ptr & 0xff);
        j60_write_ctl(ERDPTH,j60_rx_ptr >> 8);

        GPIOB->ODR &= ~(1 << 12);
        spi2_transf(rx_buf,sizeof(rx_buf),tx_buf,sizeof(tx_buf));
        GPIOB->ODR |= 1 << 12;
        j60_rx_ptr = pkt_vec->nxt_pack.addr; /* get vect status */
        uart_printf("next_addr_pack %04x size %d flags %04x get packet from %04x\r\n", 
                    pkt_vec->nxt_pack.addr, pkt_vec->pkt_size, pkt_vec->flags, j60_read_ctl(0) | (j60_read_ctl(1)<<8));
        if (pkt_vec->pkt_size > 1500) {
            /*hw in unknown state; reset it!!!*/
            //j60_clear_bit(ECON1, 4); /* clear rx bit! */
            cli_reset(NULL);
            uart_printf("eth wrong state size > 1500 so reset!\r\n");
            return;
        }
        ax_pkt_buf[0] = 0x3a; /* read operation */
        GPIOB->ODR &= ~(1 << 12);
        spi2_transf(rx_pkt_buf,pkt_vec->pkt_size,ax_pkt_buf,pkt_vec->pkt_size);
        GPIOB->ODR |= 1 << 12;
        for (j = 0; j < pkt_vec->pkt_size; j++) {
            if (j % 16 == 0) {
                uart_puts("\r\n");
            } else if (j % 8 == 0) {
                uart_puts("  ");
            }
            uart_printf("0x%02x ",rx_pkt_buf[j + 1]);
        }
        j60_write_ctl(ERXRDPTL,pkt_vec->nxt_pack.addrb[0]);
        j60_write_ctl(ERXRDPTH,pkt_vec->nxt_pack.addrb[1]);
        j60_write_ctl(ECON2, 0xc0);
        uart_puts("\r\n\r\n");
    }
}

uint8_t pkt_to_send[] = { 0x7a, /*write op*/ 0x2, /* this tell to hw set only crc */
0x33, 0x33, 0xff, 0x02, 0x28, 0x0c, 0x00, 0x04,   0xa3, 0x02, 0x28, 0x0c, 0x86, 0xdd, 0x60, 0x00,
0x00, 0x00, 0x00, 0x18, 0x3a, 0xff, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x02,   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x01, 0xff, 0x02, 0x28, 0x0c, 0x87, 0x00,   0x88, 0x06, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x80,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04,   0xa3, 0xff, 0xfe, 0x02, 0x28, 0x0c, }; //78+2

void cli_send_pkt(const char *arg)
{
    uint8_t etxndl, etxndh, estat, txstat[] = {[0 ... 8] = 0}, i;

    j60_set_bank(0);
    j60_write_ctl(EWRPTL, MEMTX_ST & 0xff);
    j60_write_ctl(EWRPTH, MEMTX_ST >> 8);
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(ax_pkt_buf, sizeof(pkt_to_send), pkt_to_send,sizeof(pkt_to_send));
    GPIOB->ODR |= 1 << 12;
    etxndl = j60_read_ctl(EWRPTL);
    etxndh = j60_read_ctl(EWRPTH);
    j60_write_ctl(ETXNDL, etxndl - 1);
    j60_write_ctl(ETXNDH, etxndh);
    //j60_clear_bit(ECON1, 4); /* clear rx bit? */
    j60_set_bit(ECON1, 8); /* set tx bit */
    do {
        sleep(1);
    } while (j60_read_ctl(ESTAT) & 8);
    /* read status */
    j60_write_ctl(ERDPTL,etxndl + 1);
    j60_write_ctl(ERDPTH,etxndh);
    ax_pkt_buf[0] = 0x3a;
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(txstat,8,ax_pkt_buf,8);
    GPIOB->ODR |= 1 << 12;
    uart_puts("pkt sent done with status: ");
    for (i = 1; i < 8; i++) {
        uart_printf("%02x ", txstat[i]);
    }
    uart_puts("\r\n\r\n");
}

/* !!! pkt SHOULD HAVE PREALOCATED TWO BYTES BEFORE UNUSED !!! */
void j60_send_pkt(char *pkt, uint16_t size)
{
    uint8_t etxndl, etxndh, estat, txstat[] = {[0 ... 8] = 0}, i;
    if (0 == (j60_cr_state & TX_COMPLETE)) {
        task_next();
    }
    j60_cr_state &= ~TX_COMPLETE;

    pkt[-1] = 0x2;
    pkt[-2] = 0x7a;
    j60_set_bank(0);
    j60_write_ctl(EWRPTL, MEMTX_ST & 0xff);
    j60_write_ctl(EWRPTH, MEMTX_ST >> 8);
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(ax_pkt_buf, size + 2, (pkt - 2),size + 2);
    GPIOB->ODR |= 1 << 12;
    etxndl = j60_read_ctl(EWRPTL);
    etxndh = j60_read_ctl(EWRPTH);
    j60_write_ctl(ETXNDL, etxndl - 1);
    j60_write_ctl(ETXNDH, etxndh);
    //j60_clear_bit(ECON1, 4); /* clear rx bit? */
    j60_set_bit(ECON1, 8); /* set tx bit */
    do {
        sleep(1);
    } while (j60_read_ctl(ESTAT) & 8);
    /* read status */
    j60_write_ctl(ERDPTL,etxndl + 1);
    j60_write_ctl(ERDPTH,etxndh);
    ax_pkt_buf[0] = 0x3a;
    GPIOB->ODR &= ~(1 << 12);
    spi2_transf(txstat,8,ax_pkt_buf,8);
    GPIOB->ODR |= 1 << 12;
}

uint8_t j60_receive_packets(j60_rcv_cb_t cb)
{
    uint8_t i, pkt_cnt, tx_buf[] = {0x3a, [ 1 ... sizeof(j60_recv_vect_t) + 1] = 0 };
    uint8_t rx_buf[] = {[0 ... sizeof(j60_recv_vect_t) + 1] = 0};
    uint16_t j;
    j60_recv_vect_t *pkt_vec = (j60_recv_vect_t *)&rx_buf[1];

    j60_set_bank(1);
    pkt_cnt = j60_read_ctl(0x19);

    if (0 == pkt_cnt) return;
#if NET_DEBUG
    uart_printf("got %d pkts!\r\n", pkt_cnt);
#endif

    j60_set_bank(0);
    for (i = 0; i < pkt_cnt; i++) {
        j60_write_ctl(ERDPTL,j60_rx_ptr & 0xff);
        j60_write_ctl(ERDPTH,j60_rx_ptr >> 8);

        GPIOB->ODR &= ~(1 << 12);
        spi2_transf(rx_buf,sizeof(rx_buf),tx_buf,sizeof(tx_buf));
        GPIOB->ODR |= 1 << 12;
        j60_rx_ptr = pkt_vec->nxt_pack.addr; /* get vect status */
#if NET_DEBUG
        uart_printf("next_addr_pack %04x size %d flags %04x get packet from %04x\r\n", 
                    pkt_vec->nxt_pack.addr, pkt_vec->pkt_size, pkt_vec->flags, j60_read_ctl(0) | (j60_read_ctl(1)<<8));
#endif
        if (pkt_vec->pkt_size > 1500) {
            /*hw in unknown state; reset it!!!*/
            //j60_clear_bit(ECON1, 4); /* clear rx bit! */
            cli_reset(NULL);
            uart_printf("eth wrong state size > 1500 so reset!\r\n");
            return 0;
        }
        if (cb) {
            /* transfer packets only when callback available */
            ax_pkt_buf[0] = 0x3a; /* read operation */
            GPIOB->ODR &= ~(1 << 12);
            spi2_transf(rx_pkt_buf,pkt_vec->pkt_size,ax_pkt_buf,pkt_vec->pkt_size);
            GPIOB->ODR |= 1 << 12;
            cb(rx_pkt_buf + 1,pkt_vec->pkt_size);
        }
#if 0
        for (j = 0; j < pkt_vec->pkt_size; j++) {
            if (j % 16 == 0) {
                uart_puts("\r\n");
            } else if (j % 8 == 0) {
                uart_puts("  ");
            }
            uart_printf("0x%02x ",rx_pkt_buf[j + 1]);
        }
#endif
        j60_write_ctl(ERXRDPTL,pkt_vec->nxt_pack.addrb[0]);
        j60_write_ctl(ERXRDPTH,pkt_vec->nxt_pack.addrb[1]);
        j60_write_ctl(ECON2, 0xc0);
        //uart_puts("\r\n\r\n");
    }
    return pkt_cnt;
}

j60_state_t j60_get_state(void)
{
    uint16_t phy_reg_val;
    /* interrupt for link does not work now */
    uint8_t int_stat = j60_read_ctl(EIR);
    if (int_stat & (1 << 1)) { /* tx err */
        uart_puts("TX error occur; try to recover!\r\n");
        j60_clear_bit(ECON1, 1 << 2); /* disable read packets */
        j60_set_bit(ECON1, 1 << 7); /* KEEP tx in reset state */
        sleep(1);
        j60_clear_bit(ECON1, 1 << 7); /* set TX in normal operation */
        sleep(1);
        j60_set_bit(ECON1, 1 << 2); /* restore reading packets */
    }
    if (int_stat & (1 << 4)) {
        phy_reg_val = spi_read_phy_reg(PHIR);
#if NET_DEBUG
        uart_printf("phir phy register is %04d\r\n", phy_reg_val);
#endif
        /* link status changed; ready phy
         * because link int is not accurate we
         * neet to confirm from phy reg */
        phy_reg_val = spi_read_phy_reg(PHSTAT2);
#if NET_DEBUG
        uart_printf("%s: cr state %d phy reg %04x\r\n", __func__, j60_cr_state, phy_reg_val);
#endif
        if (0 != (phy_reg_val & (1 << LSTAT)) && 0 == (j60_cr_state & LINK_UP)) {
                j60_cr_state = LINK_UP;
                j60_prev_state = LINK_DOWN;
                uart_puts("link up!\r\n");
                j60_cr_state |= TX_COMPLETE; /* link become up => ready to send */
                return LINK_UP;
        }
        if (0 == (phy_reg_val & (1 << LSTAT)) && 0 != (LINK_UP & j60_cr_state)) {
                j60_cr_state = LINK_DOWN;
                j60_prev_state = LINK_UP;
                uart_puts("link down!\r\n");
                return LINK_DOWN;
        }
    }
    j60_prev_state = j60_cr_state;
    if (int_stat & (1 << 3)) {
        j60_clear_bit(EIR,1 << 3);
        j60_cr_state |= TX_COMPLETE;
    }
    if (int_stat & (1 << 6)) {
        j60_cr_state |= RECV_PACK;
    }
    return j60_prev_state;
}

