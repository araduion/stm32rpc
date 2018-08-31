#include "enc28j60.h"
#include "uart.h"
#include "net.h"

#if NET_DEBUG
#define dbg_pkt(...) uart_printf(__VA_ARGS__)
#else
#define dbg_pkt(...)
#endif

macaddr_t our_mac  = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

macaddr_t peer_mac  = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

ip6addr_t our_ip6 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0};

ip6addr_t our_glob_ip6 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0};

ip6addr_t _our_glob_ip6 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     0xff, 0xfe, 0, 0, 0};

ip6addr_t peer_ip6 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0};

ip6addr_t udp_peer_ip6 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0};

macaddr_t udp_peer_mac = {0, 0, 0, 0, 0, 0};

 /* to avoid sending from multiple tasks... */
typedef struct pkts_to_send_ {
    char *pkts_buf;
    uint16_t size;
    struct pkts_to_send_ *next;
    uint8_t tid;
} pkts_to_send_t;
pkts_to_send_t pkts_to_send[10], *first_pkt = NULL;

#if 0
ip6_cache_t ip6_cache_buf[CACHE_SIZE], *ip6_cache_cr = NULL;
ip6_cache_t *ip6_cache_head, *ip6_cache_tail;
uint32_t last_age; /* < CACHE_SIZE */
#endif

/* optimized copy/cmp functions */
int inline ip6_cmp(ip6addr_t *ip6_1, ip6addr_t *ip6_2)
{
    int8_t i;
    uint32_t *a= (uint32_t*)ip6_1[0];
    uint32_t *b= (uint32_t*)ip6_2[0];
    for (i = 3; i >= 0; i--) {
        if (a[i] != b[i]) {
            return 1;
        }
    }
    return 0;
}

void inline ip6_cpy(ip6addr_t *ip6_1, ip6addr_t *ip6_2)
{
    int8_t i;
    uint32_t *a= (uint32_t*)ip6_1[0];
    uint32_t *b= (uint32_t*)ip6_2[0];
    for (i = 3; i >= 0; i--) {
        a[i] = b[i];
    }
}

int inline mac_cmp(macaddr_t *mac1, macaddr_t *mac2)
{
    int8_t i;
    uint16_t *a= (uint16_t*)mac1[0];
    uint16_t *b= (uint16_t*)mac2[0];
    for (i = 3; i >= 0; i--) {
        if (a[i] != b[i]) {
            return 1;
        }
    }
    return 0;
}

void inline mac_cpy(macaddr_t *mac1, macaddr_t *mac2)
{
    int8_t i;
    uint16_t *a= (uint16_t*)mac1[0];
    uint16_t *b= (uint16_t*)mac2[0];
    for (i = 2; i >= 0; i--) {
        a[i] = b[i];
    }
}

void calc_sum(const uint16_t *buf, int16_t len, uint32_t *sum, int ntoh)
{
    uint32_t a, c;
    uint16_t aux;

    while (len > 0) {
        if (ntoh) {
            a = ntohs(*(uint16_t *)buf);
        } else {
            a = *(uint16_t *)buf;
        }
        *sum += a;
        do {
            c = (*sum & 0xFFFF0000) >> 16;
            if (c) {
                *sum = (*sum & 0xFFFF) + c;
            }
        } while (c);
        len -= 2;
        if (len > 1) {
            buf++;
        } else if (len == 1) {
            aux = *(((uint8_t*)buf) + 2);
            buf = &aux;
        }
    }
}

uint16_t icmp6_csum(const ip6_hdr_t *ip6)
{
    uint16_t val, len;
    uint32_t sum = 0;
    ip6_option_t *opt;

    /* IPv6 pre-hdr*/
    calc_sum((uint16_t*)&ip6->src, sizeof(ip6->src), &sum, 1);
    calc_sum((uint16_t*)&ip6->dst, sizeof(ip6->dst), &sum, 1);
    val = ip6->next_hdr_type;
    len = ntohs(ip6->payload_len);
    if (val != 0x3a) { /* search for icmpv6 header */
        icmp6_hdr_t *icmp6;
        uint16_t len_netor;

        opt = (ip6_option_t *)ip6->next_hdr;
        do {
            val = opt->next_hdr_type;
            len -= (opt->len + 1) * 8;
            opt = (ip6_option_t *)opt->next_hdr;
        } while (val != 0x3a);
        /* go to icmp6 hdr */
        icmp6 = (void*)opt;
        len_netor = htons(len);

        calc_sum(&len_netor, sizeof(len_netor), &sum, 1);
        calc_sum((uint16_t*)&val, sizeof(val), &sum, 0);
        calc_sum((uint16_t*)icmp6, len, &sum, 1);
    } else {
        calc_sum(&ip6->payload_len, sizeof(ip6->payload_len), &sum, 1);
        calc_sum((uint16_t*)&val, sizeof(val), &sum, 0);
        calc_sum((uint16_t*)ip6->next_hdr, len, &sum, 1);
    }

    return (uint16_t)~sum;
}

uint16_t udp6_csum(const ip6_hdr_t *ip6)
{
    uint32_t sum = 0;
    uint16_t val, len;
    udp_hdr_t *udp;

    /* IPv6 pre-hdr*/
    calc_sum((uint16_t*)&ip6->src, sizeof(ip6->src), &sum, 1);
    calc_sum((uint16_t*)&ip6->dst, sizeof(ip6->dst), &sum, 1);
    val = ip6->next_hdr_type;
    calc_sum((uint16_t*)&val, sizeof(val), &sum, 0);
    udp = (udp_hdr_t*)ip6->next_hdr;
    len = ntohs(udp->len);
    calc_sum(&len, sizeof(udp->len), &sum, 0);
    calc_sum((const uint16_t *)udp, len, &sum, 1);
    return ~(uint16_t)sum;
}

void create_mld_nd_report(char *pack_buf)
{
    eth_mac_hdr_t *eth_hdr = (eth_mac_hdr_t *)pack_buf;
    ip6_hdr_t *ip6 = (ip6_hdr_t *)eth_hdr->next_hdr;
    ip6_option_t *ip6_hop_by_hop = (ip6_option_t *)ip6->next_hdr;
    icmp6_hdr_t *icmp6 = (icmp6_hdr_t *)ip6_hop_by_hop->next_hdr;
    icmp6_mld_report_t *mld_rep = (icmp6_mld_report_t *)icmp6->body;

    macaddr_t mcast_mac     = {0x33, 0x33, 0x00, 0x00, 0x00, 0x16};
    /* Link Local permanent address for all MLD2 capable routers; mcast routing related */
    ip6addr_t mcast_ip6_dst = {0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16};
    ip6addr_t mcast_ip6_exc = {0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00};
    _memcpy(&((uint8_t*)mcast_ip6_exc)[13], &((uint8_t*)our_mac)[3], 3);

    memset(pack_buf, 0, sizeof(pack_buf));

    mac_cpy(&eth_hdr->src, &our_mac);
    mac_cpy(&eth_hdr->dst, &mcast_mac);
    eth_hdr->next_hdr_type = htons(IP6);

    ip6->version = 0x60;
    ip6->payload_len = ntohs(sizeof(icmp6_option_t) + sizeof(icmp6_hdr_t) + sizeof(icmp6_mld_report_t));
    ip6->next_hdr_type = 0; /* hop by hop option */
    ip6->hop_limit = 0x01;
    ip6_cpy(&ip6->dst, &mcast_ip6_dst);

    ip6_hop_by_hop->next_hdr_type = 0x3a;
    ip6_hop_by_hop->len = 0;
    ip6_hop_by_hop->ralert = IP6_ROUTER_ALERT;
    ip6_hop_by_hop->padding = 0x1;

    icmp6->type = 0x8F;
    icmp6->code = 0;

    mld_rep->num_rec = ntohs(1);
    mld_rep->group[0].type = 0x04;
    mld_rep->group[0].auth_len = 0;
    ip6_cpy(&mld_rep->group[0].mcast_addr, &mcast_ip6_exc);
    icmp6->checksum = 0;
    icmp6->checksum = htons(icmp6_csum(ip6));
}

/* write packet content on pack_buf and return size */
uint16_t create_neigh_adv(char *pack_buf, eth_mac_hdr_t *pack, ip6_hdr_t *pIp6, icmp6_hdr_t *pIcmp6,
                      icmp6_nbr_advertisment_t *pSol)
{
    eth_mac_hdr_t *eth_hdr = (eth_mac_hdr_t *)pack_buf;
    ip6_hdr_t *ip6;
    icmp6_hdr_t *icmp6;
    icmp6_option_t *icmp6_option;
    icmp6_nbr_advertisment_t *icmp6_adv;

    /* L2 init */
    mac_cpy(&eth_hdr->dst, &pack->src);
    mac_cpy(&eth_hdr->src, &our_mac);
    eth_hdr->next_hdr_type = htons(IP6);
    ip6 = (ip6_hdr_t *)eth_hdr->next_hdr;
    /* L3 init */
    ip6_cpy(&ip6->dst, &pIp6->src);
    ip6_cpy(&ip6->src, &our_ip6);
    ip6->version = IP6VER;
    ip6->payload_len = htons(sizeof(icmp6_hdr_t) + sizeof(icmp6_nbr_advertisment_t) + sizeof(icmp6_option_t));
    ip6->hop_limit = 255;
    ip6->next_hdr_type = ICMP6;
    icmp6 = (icmp6_hdr_t *)ip6->next_hdr;
    /* L4 */
    icmp6->type = NEIGHBOR_ADVERT;
    icmp6->code = 0;
    icmp6_adv = (icmp6_nbr_advertisment_t *)icmp6->body;
    icmp6_adv->flags = htonl(ICMP6_FLAGS_SOLICITED | ICMP6_FLAGS_OVERRIDE);
    ip6_cpy(&icmp6_adv->target, &pSol->target);
    /* option header */
    icmp6_option = (icmp6_option_t *)icmp6_adv->options;
    icmp6_option->type = 0x02; /* Target link address */
    icmp6_option->len = 1; /* x 8 bytes */
    mac_cpy(&icmp6_option->data, &our_mac);
    icmp6->checksum = 0; /* required for csum calc */
    icmp6->checksum = htons(icmp6_csum(ip6));
    return (sizeof(eth_mac_hdr_t) + sizeof(ip6_hdr_t) + sizeof(icmp6_hdr_t) + sizeof(icmp6_nbr_advertisment_t)
             + sizeof(icmp6_option_t));
}

uint16_t create_rtr_sol(char *pack_buf)
{
    macaddr_t rsol_mcast_mac = {0x33, 0x33, 0xff, 0, 0, 2};
    eth_mac_hdr_t *eth_hdr = (eth_mac_hdr_t *)pack_buf;
    ip6_hdr_t *ip6;
    icmp6_hdr_t *icmp6;
    ip6addr_t rsol_mcast_ip6 = {0xff,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,2};
    icmp6_rtr_sol_t *rsol;

    mac_cpy(&eth_hdr->dst, &rsol_mcast_mac);
    mac_cpy(&eth_hdr->src, &our_mac);
    eth_hdr->next_hdr_type = htons(IP6);
    ip6 = (ip6_hdr_t *)eth_hdr->next_hdr;
    ip6->version = IP6VER;
    ip6_cpy(&ip6->dst, &rsol_mcast_ip6);
    ip6_cpy(&ip6->src, &our_ip6);
    ip6->payload_len = htons(sizeof(icmp6_hdr_t) + sizeof(icmp6_rtr_sol_t));
    ip6->version = IP6VER;
    ip6->next_hdr_type = ICMP6;
    ip6->hop_limit = 0xff;
    /* L4 */
    icmp6 = (icmp6_hdr_t *)ip6->next_hdr;
    icmp6->type = ROUTER_SOLICIT;
    icmp6->code = 0;
    rsol = (icmp6_rtr_sol_t *)icmp6->body;
    rsol->reserved = 0;
    rsol->type = 1;
    rsol->len = 1;
    mac_cpy(&rsol->src_mac,&our_mac);
    icmp6->checksum = 0;
    icmp6->checksum = htons(icmp6_csum(ip6));
    return sizeof(eth_mac_hdr_t) + sizeof(ip6_hdr_t) + sizeof(icmp6_hdr_t) + sizeof(icmp6_rtr_sol_t);
}

uint16_t create_neigh_sol(char *pack_buf, ip6addr_t target)
{
    eth_mac_hdr_t *eth_hdr = (eth_mac_hdr_t *)pack_buf;
    ip6_hdr_t *ip6;
    icmp6_hdr_t *icmp6;
    icmp6_nbr_advertisment_t *icmp6_adv;
    icmp6_option_t *icmp6_option;

    macaddr_t nsol_mcast_mac = {0x33, 0x33, 0xff, 0, 0, 0};
    _memcpy(&((uint8_t*)nsol_mcast_mac)[3], &((uint8_t*)target)[13], 3);
    ip6addr_t nsol_mcast_ip6 = {0xff, 0x02, 0,0,0,0,0,0,0,0, 0x0, 0x1, 0xff, 0x0, 0x0, 0x0};
    _memcpy(&((uint8_t*)nsol_mcast_ip6)[13], &((uint8_t*)target)[13], 3);

    /* L2 init */
    memset(pack_buf, 0, sizeof(pack_buf));
    mac_cpy(&eth_hdr->dst, &nsol_mcast_mac);
    mac_cpy(&eth_hdr->src, &our_mac);
    eth_hdr->next_hdr_type = htons(IP6);
    /* L3 init */
    ip6 = (ip6_hdr_t *)eth_hdr->next_hdr;
    ip6->version = IP6VER;
    ip6_cpy(&ip6->dst, &nsol_mcast_ip6);
    ip6_cpy(&ip6->src, &our_ip6);
    ip6->payload_len = htons(sizeof(icmp6_hdr_t) + sizeof(icmp6_nbr_advertisment_t) + sizeof(icmp6_option_t));
    ip6->next_hdr_type = ICMP6;
    ip6->hop_limit = 0xff;
    /* L4 */
    icmp6 = (icmp6_hdr_t *)ip6->next_hdr;
    icmp6->type = NEIGHBOR_SOLICIT;
    icmp6->code = 0;
    icmp6_adv = (icmp6_nbr_advertisment_t *)icmp6->body;
    ip6_cpy(&icmp6_adv->target, (ip6addr_t*)&target);
    icmp6_option = (icmp6_option_t *)icmp6_adv->options;
    /* option header */
    icmp6_option->type = 1; /* source link */
    icmp6_option->len = 1;
    mac_cpy(&icmp6_option->data, &our_mac);
    icmp6->checksum = 0;
    icmp6->checksum = htons(icmp6_csum(ip6));
    return sizeof(eth_mac_hdr_t) + sizeof(ip6_hdr_t) + sizeof(icmp6_hdr_t) +
           sizeof(icmp6_nbr_advertisment_t) + sizeof(icmp6_option_t);
}

uint16_t create_ping_reply(char *pack_buf, eth_mac_hdr_t *pack, ip6_hdr_t *pIp6,
                        icmp6_hdr_t *pIcmp6, icmp6_ping_t *pPingHdr)
{
    eth_mac_hdr_t *eth_hdr = (eth_mac_hdr_t *)pack_buf;
    ip6_hdr_t *ip6;
    icmp6_hdr_t *icmp6;
    icmp6_option_t *icmp6_option;

    /* L2 init */
    mac_cpy(&eth_hdr->dst, &pack->src);
    mac_cpy(&eth_hdr->src, &our_mac);
    eth_hdr->next_hdr_type = htons(IP6);
    ip6 = (ip6_hdr_t *)eth_hdr->next_hdr;
    /* L3 init */
    ip6_cpy(&ip6->dst, &pIp6->src);
    ip6_cpy(&ip6->src, &pIp6->dst);
    ip6->version = IP6VER;
    ip6->payload_len = pIp6->payload_len;
    ip6->hop_limit = pIp6->hop_limit;
    ip6->next_hdr_type = pIp6->next_hdr_type;
    icmp6 = (icmp6_hdr_t *)ip6->next_hdr;
    /* L4 processing */
    _memcpy(icmp6, pIcmp6, ntohs(pIp6->payload_len));
    icmp6->type = ECHO_REPLY;
    icmp6->code = 0;
    icmp6->checksum = 0;
    icmp6->checksum = htons(icmp6_csum(ip6));
    return sizeof(eth_mac_hdr_t) + sizeof(ip6_hdr_t) + sizeof(icmp6_hdr_t) + ntohs(pIp6->payload_len) - 4;
}

//#define IS_LINK_LOCAL_IP6(addr)  (((char*)addr)[0]         == 0xfe &&  (((char*)addr)[1]         & (1 << 7)) != 0)
  #define IS_LINK_LOCAL_IP6(addr) ((((char*)addr)[0] & 0xff) == 0xfe && ((((char*)addr)[1] & 0xff) & (1 << 7)) != 0)

uint16_t create_udp_packet(char *pack_buf, macaddr_t *dst_mac, ip6addr_t *dst_ip6, void *data, uint16_t size)
{
    eth_mac_hdr_t *eth_hdr = (eth_mac_hdr_t *)pack_buf;
    ip6_hdr_t *ip6;
    udp_hdr_t *udp;

    mac_cpy(&eth_hdr->dst, dst_mac);
    mac_cpy(&eth_hdr->src, &our_mac);
    eth_hdr->next_hdr_type = htons(IP6);
    ip6 = (ip6_hdr_t *)eth_hdr->next_hdr;
    /* L3 init */
    ip6_cpy(&ip6->dst, (ip6addr_t*)dst_ip6);
    if (IS_LINK_LOCAL_IP6(*dst_ip6)) {
        ip6_cpy(&ip6->src, &our_ip6);
    } else {
        ip6_cpy(&ip6->src, &our_glob_ip6);
    }
    ip6->version = IP6VER;
    ip6->payload_len = htons(sizeof(udp_hdr_t) + size);
    ip6->hop_limit = 64;
    ip6->next_hdr_type = UDP;
    udp = (udp_hdr_t *)ip6->next_hdr;
    udp->sport = htons(4000);
    udp->dport = htons(4000);
    udp->len = htons(size + sizeof(udp_hdr_t));
    _memcpy(udp->body, data, size);
    udp->csum = 0;
    udp->csum = htons(udp6_csum(ip6));
    return sizeof(eth_mac_hdr_t) + sizeof(ip6_hdr_t) + sizeof(udp_hdr_t) + size;
}

/* cannot send packet from here because we are in CLI task! */
void cli_send_udp(const char *arg)
{
    uint16_t pack_size;
    char data[] = "Hello world!";
    if (*(uint32_t*)&udp_peer_ip6[0] == 0) {
        uart_puts("No ipv6 udp peer set!\r\n");
        return;
    }
    /* send beginning with offset 98 to avoid overlap with NS/NA */
    pack_size = create_udp_packet(&tx_pkt_buf[100], &udp_peer_mac, &udp_peer_ip6, &data, sizeof(data));

    if (NULL == first_pkt) {
        first_pkt = &pkts_to_send[0];
        first_pkt->next = NULL;
        first_pkt->pkts_buf = &tx_pkt_buf[100];
        first_pkt->size = pack_size;
        first_pkt->tid = cr_task->task_id;
    }

    /* DO NOT SEND PACKET HERE BECAUSE WE ARE IN CLI TASK!!!
    j60_send_pkt(&tx_pkt_buf[2], pack_size);
    uart_printf("sent %d bytes!\r\n", pack_size);*/
}

void printf_ip6_addr(const char *text, ip6addr_t *pAddr)
{
    unsigned char *addr = (unsigned char *)pAddr;
    uart_printf("%s %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\r\n", text,
            addr[0], addr[1], addr[2], addr[3],
            addr[4], addr[5], addr[6], addr[7],
            addr[8], addr[9], addr[10], addr[11],
            addr[12], addr[13], addr[14], addr[15]
            );
}


void process_incoming_proto(eth_mac_hdr_t *pack, uint16_t size)
{
    ip6_hdr_t *ip6;
    ip6_option_t *opt;
    icmp6_ping_t *ping;
    icmp6_hdr_t *icmp6;
    icmp6_nbr_advertisment_t *sol, *adv;
    icmp6_rtr_advertisment_t *rtr_adv;
    icmp6_rtr_adv_prefix_opt_t *rtr_prefix;
    uint16_t pack_size;
    uint8_t *ptr;
    ip6addr_t *rtr_pref_addr;
    icmp6_mld_report_t *mlt_rep;
    uint16_t i,udp_len;
    udp_hdr_t *udp;
    char buf[20];
    uint16_t csum, calc_csum;
    uint8_t icmp_type;
    bool found_prefix = false;

    uint16_t l3_proto = ntohs(pack->next_hdr_type);
    switch (l3_proto) {
        case IP6:
            ip6 = (ip6_hdr_t *)(pack->next_hdr);
            uint8_t l4_proto = ip6->next_hdr_type;
            switch (l4_proto) {
                case ICMP6:
                    icmp6 = (icmp6_hdr_t *)(ip6->next_hdr);
                    icmp_type = icmp6->type;
                    csum = ntohs(icmp6->checksum);
                    icmp6->checksum = 0;
                    calc_csum = icmp6_csum(ip6);
                    if (calc_csum == csum) {
                        dbg_pkt("%s: checksum %x ok!\r\n", __func__, csum);
                        icmp6->checksum = htons(csum);
                    } else {
                        dbg_pkt("%s: checksum error %x instead of %x!\r\n", __func__, csum, calc_csum);
                    }
                    switch (icmp6->type) {
                        case NEIGHBOR_SOLICIT:
                            sol = (icmp6_nbr_advertisment_t *)icmp6->body;
                            if (ntohl(sol->flags) & ICMP6_FLAGS_ROUTER) {
                                dbg_pkt("%s: router flag!\r\n", __func__);
                            }
                            if (ntohl(sol->flags) & ICMP6_FLAGS_SOLICITED) {
                                dbg_pkt("%s: solicited flag!\r\n", __func__);
                            }
                            if (ntohl(sol->flags) & ICMP6_FLAGS_OVERRIDE) {
                                dbg_pkt("%s: override flag!\r\n", __func__);
                            }
                            dbg_pkt("cmp:\r\n");
#if NET_DEBUG
                            printf_ip6_addr("our_ip6", &our_ip6);
                            printf_ip6_addr("target", &sol->target);
#endif
                            if (0 == ip6_cmp(&our_ip6, &sol->target) ||
                                0 == ip6_cmp(&our_glob_ip6, &sol->target)) {
                                dbg_pkt("send adv for our address!\r\n");
                                pack_size = create_neigh_adv(&tx_pkt_buf[2], pack, ip6, icmp6, sol);
                                j60_send_pkt(&tx_pkt_buf[2],pack_size);
                            } else {
                                dbg_pkt("not me, ignore it!\r\n");
                            }
                        break;
                        case NEIGHBOR_ADVERT:
                            adv = (icmp6_nbr_advertisment_t *)icmp6->body;
#if NET_DEBUG
                            printf_ip6_addr("got nbr advertisment target", &adv->target);
#endif
                            if (ntohl(adv->flags) & ICMP6_FLAGS_ROUTER) {
                                dbg_pkt("%s: router flag!\r\n", __func__);
                            }
                            if (ntohl(adv->flags) & ICMP6_FLAGS_SOLICITED) {
                                dbg_pkt("%s: solicited flag!\r\n", __func__);
                            }
                            if (ntohl(adv->flags) & ICMP6_FLAGS_OVERRIDE) {
                                dbg_pkt("%s: override flag!\r\n", __func__);
                            }
                            ip6_cpy(&peer_ip6, &adv->target);
                        break;
                        case ROUTER_ADVERT:
                            if (our_ip6[0] == 0) {
                                dbg_pkt("ignore advertisment due to no link-local address!\r\n");
                                break;
                            }
                            if (j60_cr_state & GLOB_ADDR) {
                                break;
                            }
                            rtr_adv = (icmp6_rtr_advertisment_t *)icmp6->body;
                            dbg_pkt("%s: got rtr advertisment lifetime %d\r\n", __func__, ntohs(rtr_adv->router_lifetime));
                            pack_size = ntohs(ip6->payload_len); /* check for end of packet */
                            pack_size -= (sizeof(icmp6_hdr_t) + sizeof(icmp6_rtr_advertisment_t));
                            ptr = &rtr_adv->option_type[0];
                            do {
                                switch (*ptr) {
                                    case RTR_PREFIX_INFO:
                                        rtr_prefix = (icmp6_rtr_adv_prefix_opt_t*)ptr;
                                        dbg_pkt("get prefix len %d valid %d pref %d\r\n",
                                                rtr_prefix->prefix_length, ntohl(rtr_prefix->valid_lifetime),
                                                ntohl(rtr_prefix->preferred_lifetime));
#if NET_DEBUG
                                        printf_ip6_addr("prefix",&rtr_prefix->prefix);
#endif
                                        rtr_pref_addr = &rtr_prefix->prefix;
                                        pack_size -= rtr_prefix->length*8;
                                        ptr += rtr_prefix->length*8;
                                        found_prefix = true;
                                    break;
                                    case RTR_LINK_ADDR:
                                        /* if we got here.. ok... print it*/
                                        dbg_pkt("mac from link layer %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                                                ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]);
                                        pack_size -= 8 * (*(ptr + 1));
                                        ptr += ((*(ptr + 1)) * 8);
                                    break;
                                    case RTR_RECURS_DNS_SRV: /* RFC6106 second byte is len */
                                    case RTR_DNS_SRC_LIST:
                                        pack_size -= 8 * (*(ptr + 1));
                                        ptr += 8 * (*(ptr + 1));
                                    break;
                                    default:
                                        /* skip unknown icmpv6 options */
                                        pack_size -= 8 * (*(ptr + 1));
                                        ptr += ((*(ptr + 1)) * 8);
                                }
                            } while(pack_size > 0);
                            if (false == found_prefix) break; /* no ipv6 prefix found! */
                            _memcpy(_our_glob_ip6, rtr_pref_addr, rtr_prefix->prefix_length/8);
                            _our_glob_ip6[8] = our_mac[0];
                            _our_glob_ip6[9] = our_mac[1];
                            _our_glob_ip6[10] = our_mac[2];
                            _our_glob_ip6[13] = our_mac[3];
                            _our_glob_ip6[14] = our_mac[4];
                            _our_glob_ip6[15] = our_mac[5];
                            if ((_our_glob_ip6[8] & 2) == 0) {
                                _our_glob_ip6[8] |= 2;
                            } else {
                                _our_glob_ip6[8] &= ~2;
                            }
                            printf_ip6_addr("our global ipv6 address to be set:", &_our_glob_ip6);
                        break;
                        case ECHO_REQUEST:
                            ping = (icmp6_ping_t*)icmp6->body;
                            dbg_pkt("%s: ping req seq %d identifier %x\r\n", __func__,
                                        ntohs(ping->seq), ntohs(ping->identifier));
                            pack_size = create_ping_reply(&tx_pkt_buf[2], pack, ip6, icmp6, (icmp6_ping_t *)icmp6->body);
                            j60_send_pkt(&tx_pkt_buf[2],pack_size);
                        break;
                        case ECHO_REPLY:
                            ping = (icmp6_ping_t*)icmp6->body;
                            dbg_pkt("%s: ping reply seq %d identifier %x\r\n", __func__,
                                        ntohs(ping->seq), ntohs(ping->identifier));
                        break;
                        case MLD_REPORT:
                        break;
                        default:
                            dbg_pkt("%s: ICMP6 type 0x%x not supported!\n", __func__, icmp6->type);
                    }
                break;
                case HOPBYHOP:
                    /* we should find MLD after this */
                    opt = (ip6_option_t *)ip6->next_hdr;
                    icmp6 = (icmp6_hdr_t *)opt->next_hdr;
                    switch (icmp6->type) {
                        case MLD_REPORT:
#if NET_DEBUG
                            printf_ip6_addr("mld received from:", &ip6->src);
                            mlt_rep = (icmp6_mld_report_t *)&icmp6->body[0];
                            for (i = 0; i < ntohs(mlt_rep->num_rec); i++)
                                printf_ip6_addr("mld group address:", &mlt_rep->group[i].mcast_addr);
#endif
                        break;
                    }
                break;
                case UDP:
                    mac_cpy((macaddr_t*)&udp_peer_mac, &pack->src);
                    udp = (udp_hdr_t*)ip6->next_hdr;
                    csum = udp->csum;
                    udp->csum = 0;
                    calc_csum = htons(udp6_csum(ip6));
                    if (calc_csum != csum) {
                        dbg_pkt("%s: received udp with wrong csum %04x should be %04x\r\n",
                                __func__, csum, calc_csum);
                        break;
                    }
                    ip6_cpy(&udp_peer_ip6, &ip6->src);
                    udp_len = ntohs(udp->len);
                    dbg_pkt("got udp sport %d dport %d len %d\r\n", ntohs(udp->sport),
                                ntohs(udp->dport), udp_len);
                    set_power(udp->body,udp_len - sizeof(udp_hdr_t));
                    pack_size = create_udp_packet(&tx_pkt_buf[2], &udp_peer_mac,
                            &udp_peer_ip6, udp->body, udp_len - sizeof(udp_hdr_t));
                    j60_send_pkt(&tx_pkt_buf[2],pack_size);
#if 0
                    if (udp_len - sizeof(udp_hdr_t) < sizeof(buf)) {
                        _memcpy(buf,udp->body,udp_len - sizeof(udp_hdr_t));
                        buf[udp_len] = '\0';
                        dbg_pkt("UDP text: %s\r\n", buf);
                    }
#endif
                break;
                default:
                    dbg_pkt("%s: L4 protocol 0x%x not supported!\r\n", __func__, l4_proto);
            }
        break;
        default:
            dbg_pkt("%s: L3 protocol 0x%x not supported!\r\n", __func__, l3_proto);
    }
}

void net_start(void *arg)
{
    uint8_t i;
    uint16_t pack_size;
    ip6addr_t lladdr = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xfe,
                        0, 0, 0};

    set_power_pin_state();
    spi2_init(0);
    enc28j60_init(NULL);
    if (j60_eth_is_init) {
        uart_puts("eth enc28j60 ready!\r\n\r");
    } else {
        uart_puts("enc28j60 init failed!\r\n\r");
        while (1) {
            task_next();
        }
    }
    lladdr[8] = our_mac[0];
    lladdr[9] = our_mac[1];
    lladdr[10] = our_mac[2];
    lladdr[13] = our_mac[3];
    lladdr[14] = our_mac[4];
    lladdr[15] = our_mac[5];
    if ((lladdr[8] & 2) == 0) {
        lladdr[8] |= 2;
    } else {
        lladdr[8] &= ~2;
    }
    printf_ip6_addr("link-local", &lladdr);
    while (1) {
        if (j60_cr_state & DAD_FAIL) {
            continue;
        }
        if (LINK_DOWN == j60_cr_state) {
            if (LINK_UP & j60_prev_state) {
                /* drop packets */
                j60_receive_packets(NULL);
                j60_prev_state = LINK_DOWN;
            }
            j60_get_state();
            continue;
        }
        if (0 == (LINK_UP & j60_prev_state) && 0 != (LINK_UP & j60_cr_state)) {
            pack_size = create_neigh_sol(&tx_pkt_buf[2], lladdr);
            j60_send_pkt(&tx_pkt_buf[2],pack_size);
            sleep(10);
            j60_prev_state = j60_cr_state;
            continue;
        }
        if (0 == (j60_cr_state & LINK_LOCAL_ADDR)) {
            memset(peer_ip6,0,sizeof(ip6addr_t));
            j60_receive_packets((j60_rcv_cb_t)process_incoming_proto);
            if (0 == ip6_cmp(&peer_ip6, &lladdr)) {
                j60_cr_state |= DAD_FAIL;
                uart_puts("DAD failed!\r\n");
                continue;
            } else {
                ip6_cpy(&our_ip6, &lladdr);
                j60_cr_state |= LINK_LOCAL_ADDR;
                pack_size = create_rtr_sol(&tx_pkt_buf[2]);
                j60_send_pkt(&tx_pkt_buf[2],pack_size);
                sleep(10);
                j60_receive_packets((j60_rcv_cb_t)process_incoming_proto);
                continue;
            }
        }
        if (*(uint32_t*)&_our_glob_ip6[0] && 0 == (j60_cr_state & GLOB_ADDR) &&
            0 == (j60_cr_state & DAD_GLOB_FAIL)) {
            pack_size = create_neigh_sol(&tx_pkt_buf[2], _our_glob_ip6);
            j60_send_pkt(&tx_pkt_buf[2],pack_size);
            sleep(10);
            memset(peer_ip6,0,sizeof(ip6addr_t));
            j60_receive_packets((j60_rcv_cb_t)process_incoming_proto);
            if (0 == ip6_cmp(&peer_ip6, &_our_glob_ip6)) {
                j60_cr_state |= DAD_GLOB_FAIL;
                uart_puts("DAD glob failed!\r\n");
                continue;
            } else {
                ip6_cpy(&our_glob_ip6, &_our_glob_ip6);
                j60_cr_state |= GLOB_ADDR;
            }
        }
        if (first_pkt) {
            for (; first_pkt; first_pkt = first_pkt->next) {
                if (first_pkt->size <= 1500) {
                    j60_send_pkt(first_pkt->pkts_buf, first_pkt->size);
                    dbg_pkt("send packet from other task size %d tid %d\r\n",
                                first_pkt->size, first_pkt->tid);
                } else {
                    uart_printf("packet to be to be sent from task %d too big %d!\r\n",
                            first_pkt->tid, first_pkt->size);
                }
            }
            memset(&pkts_to_send[0], 0, sizeof(pkts_to_send));
        }
        j60_get_state();
        if (j60_cr_state & RECV_PACK) {
            j60_receive_packets((j60_rcv_cb_t)process_incoming_proto);
        } else {
            task_next();
        }
    }
}

