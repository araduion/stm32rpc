#ifndef __ENC28J60_H__
#define __ENC28J60_H__

#include "common.h"
#include "net.h"

#define MII_L 0x18
#define MII_H 0x19

#define MEMRX_ST 0x0000
#define MEMRX_ND 0x0fff
#define MEMTX_ST 0x1000 /* 4k */
#define MEMTX_ND 0x1fff /* 8k */

/* Common Bank registers */
#define EIE   0x1B
#define EIR   0x1C
#define ESTAT 0x1D
#define ECON2 0x1E
#define ECON1 0x1F

/* ECON1 bits */
#define BSEL0   0
#define BSEL1   1
#define ACT_RX  2
#define TXRTS   3
#define CSUMEN  4 /*DMA checksum enable*/
#define DMAST   5
#define RXRST   6
#define TXRST   7

#define EPKTCNT 0x19 /* bank 1*/
#define ERXFCON 0x18
/* Transmit buffer set register */
#define ETXSTL 0x04
#define ETXSTH 0x05
#define ETXNDL 0x06
#define ETXNDH 0x07
#define EDMASTL 0x10
#define EDMASTH 0x11
#define EDMANDL 0x12
#define EDMANDH 0x13
#define EDMACSL 0x16
#define EDMACSH 0x17

/* Receive buffer set register */
#define ERXSTL 0x08
#define ERXSTH 0x09
#define ERXNDL 0x0A
#define ERXNDH 0x0B
#define ERXRDPTL 0x0C
#define ERXRDPTH 0x0D
#define ERXWRPTL 0X0E
#define ERXWRPTH 0X0F

/* current location in receive buffer */
#define ERXRDPT
/* Read pionter */
#define ERDPTH 0x01
#define ERDPTL 0x00
/* Write pointer */
#define EWRPTH 0x03
#define EWRPTL 0x02

/* MACON1 bits */
#define MARXEN  0
#define PASSALL 1
#define RXPAUS  2
#define TXPAUS  3
#define LOOPBK  4

/* MACON2 bits*/
#define TFUNRST 0
#define MATXRST 1
#define RFUNRST 2
#define MARXRST 3
#define RNDRST  6
#define MARST   7

#define MABBIPG 4
#define MAIPGL 6
#define MAIPGH 7

/* MACON3 bits */
#define FULDPX 0
#define FRMLEN 1
#define TXCRCEN 4
/* MACON3 bits */
#define DEFER 6


/* define end tx buff */
#define END_TX_BUF 0x0be9
#define START_RX_BUF 0x0bf9
/* phy control regs pg. 22*/
#define PHCON1 0
#define PHCON2 0x10
#define PDPXMD 8
#define DPXSTAT 9
#define LSTAT 10
#define PHSTAT1 1
#define PHSTAT2 0x11
#define PHLCON 0x14
#define PHID1 0x2
#define PHID2 0x3
#define PHIE 0x12
#define PHIR 0x13

/* reg to control mii bank 2*/
#define MICMD 0x12
#define MIRDL 0x18
#define MIRDH 0x19
#define MIWRL 0x16
#define MIWRH 0x17
#define MIREGADR 0x14
/* mtu in bank2 */
#define MAMXFLL 0xa
#define MAMXFLH 0xb
/* MAC Registers */
#define MACON1 0x00
#define MACON2 0x01
#define MACON3 0x02
#define MACON4 0x03


/* bank 3 */
#define MAADR5 0
#define MAADR6 1
#define MAADR3 2
#define MAADR4 3
#define MAADR1 4
#define MAADR2 5
#define REVID 0x12
#define EBSTSD 0x6
#define EBSTCON 0X7
#define EBSTCSL 0x8
#define EBSTCSH 0x9
#define MISTAT 0x0A
#define EREVID 0x12

#define MTU 1500

typedef struct j60_recv_vect_t_ {
    union {
        uint8_t addrb[2];
        uint16_t addr;
    } nxt_pack; /* should be decremented by 10 to
        hit next status vector and next addr*/
    uint16_t pkt_size;
    uint8_t flags;
} __attribute__((packed)) j60_recv_vect_t;

typedef enum j60_state_ {
    LINK_DOWN = 0,
    LINK_UP = 1,
    LINK_LOCAL_ADDR = 2,
    GLOB_ADDR = 4,
    RECV_PACK = 8,
    TX_COMPLETE = 16,
    DAD_FAIL = 32,
    DAD_GLOB_FAIL = 64
} j60_state_t;

typedef void (*j60_rcv_cb_t)(eth_mac_hdr_t *,uint16_t);

extern bool j60_eth_is_init;
extern j60_state_t j60_prev_state;
extern j60_state_t j60_cr_state;

void enc28j60_init(const char *);
void cli_setled(const char *arg);
void cli_reset(const char *arg);
void cli_read_phy_reg(const char *arg);
void cli_enc28_selftest(const char *arg);
j60_state_t j60_get_state(void);
void cli_enc28_read(const char *arg);
void cli_enc28_write(const char *arg);
void cli_read_ctrl_reg(const char *arg);
void cli_write_ctrl_reg(const char *arg);
void set_read_dbg(const char *arg);
void cli_enc28_dump_ptr(const char *arg);
void cli_enc28_dump_pkts(const char *arg);
void cli_send_pkt(const char *arg);
uint8_t j60_receive_packets(j60_rcv_cb_t cb);
void j60_send_pkt(char *pkt, uint16_t size);

#endif

