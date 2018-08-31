#ifndef __NET_H__
#define __NET_H__

#include "common.h"

#define IP6 0x86DD
#define IP6VER 0x60
#define ICMP6 0x3a
#define HOPBYHOP 0
#define UDP 0x11

typedef uint8_t ip6addr_t[16];
typedef uint8_t macaddr_t[6];

#if 0
typedef struct ip6_cache_ {
    ip6addr_t addr;
    macaddr_t mac;
    uint32_t age; /* how old is it */
    struct ip6_cache_ *next, *prev;
} __attribute__((packed)) ip6_cache_t;
#endif

#define CACHE_SIZE 16

typedef enum __attribute__((packed)) icmp6_type_ {
    ECHO_REQUEST = 0x80,
    ECHO_REPLY = 0x81,
    ROUTER_SOLICIT = 0x85,
    ROUTER_ADVERT = 0x86,
    NEIGHBOR_SOLICIT = 0x87,
    NEIGHBOR_ADVERT = 0x88,
    REDIRECT = 0x89,
    MLD_REPORT = 0x8F
} icmp6_type_t;

typedef struct eth_mac_hdr_ {
    macaddr_t dst;
    macaddr_t src;
    uint16_t next_hdr_type;
    uint8_t next_hdr[0];
} __attribute__((packed)) eth_mac_hdr_t;

typedef struct ip6_hdr_ {
    uint8_t version; /* only first 4 bits are version;
                         the rest (traffic class and flow label) not interesting */
    uint8_t tc[3]; /* traffic class + flow label */
    uint16_t payload_len;
    uint8_t next_hdr_type; /* ICMP6, TCP, UDP, ... */
    uint8_t hop_limit;
    ip6addr_t src;
    ip6addr_t dst;
    char next_hdr[0];
} __attribute__((packed)) ip6_hdr_t;

#define is_ip6(ip_hdr) (ip_hdr->version[0] == 0x60)

typedef struct icmp6_hdr_ {
    icmp6_type_t type;
    uint8_t code;
    uint16_t checksum;
    char body[0];
} __attribute__((packed)) icmp6_hdr_t;

#define ICMP6_FLAGS_ROUTER    0x10000000
#define ICMP6_FLAGS_SOLICITED 0x20000000
#define ICMP6_FLAGS_OVERRIDE  0x40000000
#define IP6_ROUTER_ALERT 0x00000205

typedef struct icmp6_nbr_advertisment_ { /* also used for solicit */
    uint32_t flags; /* Router Flag, Solicited Flag, Override Flag,
                       other 29 bits reserved.. */
    ip6addr_t target; /* sometimes is sender... proxyarp...*/
    char options[0];
} __attribute__((packed)) icmp6_nbr_advertisment_t;

typedef enum rtr_adv_option_type_ {
    RTR_LINK_ADDR = 1,
    RTR_PREFIX_INFO = 3,
    RTR_ROUTE_INFO = 24,
    RTR_RECURS_DNS_SRV = 25,
    RTR_DNS_SRC_LIST = 31,
} rtr_adv_option_type_t;

typedef struct icmp6_rtr_advertisment_ {
    uint8_t cr_hop_limit;
    uint8_t flags;
    uint16_t router_lifetime;
    uint32_t reachable_time;
    uint32_t retrans_time;
    char option_type[0];
} __attribute__((packed)) icmp6_rtr_advertisment_t;

typedef struct icmp6_rtr_adv_prefix_opt_ {
    uint8_t type;
    uint8_t length;
    uint8_t prefix_length;
    uint8_t onlink_flags;
    uint32_t valid_lifetime;
    uint32_t preferred_lifetime;
    uint32_t reserved;
    ip6addr_t prefix;
} __attribute__((packed)) icmp6_rtr_adv_prefix_opt_t;

typedef struct icmp6_ping_ {
    uint16_t identifier;
    uint16_t seq;
    char data[0];
} __attribute__((packed)) icmp6_ping_t;

typedef struct icmp6_option_ {
    uint8_t type;
    uint8_t len; /* x 8 bytes */
    macaddr_t data;
    char next_hdr[0];
} __attribute__((packed)) icmp6_option_t;

typedef struct icmp6_rtr_sol_ {
    uint32_t reserved; /* zero */
    uint8_t type; /* =1 */
    uint8_t len;  /* =1 => 8 bytes */
    macaddr_t src_mac;
} icmp6_rtr_sol_t;

typedef struct __attribute__((packed)) ip6_option_ {
    uint8_t next_hdr_type;
    uint8_t len; /* 0 = 8, 1 = 16, ... */
    uint32_t ralert;
    uint16_t padding;
    char next_hdr[0];
} __attribute__((packed)) ip6_option_t;

typedef struct icmp6_mld_group_ {
    uint8_t type;
    uint8_t auth_len;
    uint16_t num_src;
    ip6addr_t mcast_addr;
} icmp6_mld_group_t;

typedef struct icmp6_mld_report_ { /* MLD_REPORT */
    uint16_t zero;
    uint16_t num_rec;
    icmp6_mld_group_t group[1]; /* minimum 1; size if given by prev field! */
} __attribute__((packed)) icmp6_mld_report_t;

typedef struct udp_hdr_ {
    uint16_t sport;
    uint16_t dport;
    uint16_t len;
    uint16_t csum;
    char body[0];
} __attribute__((packed)) udp_hdr_t;

extern char rx_pkt_buf[];
extern char tx_pkt_buf[];

extern macaddr_t our_mac;
extern ip6addr_t udp_peer_ip6;
extern macaddr_t udp_peer_mac;

void net_start(void *arg);
void cli_send_udp(const char *arg);

#endif
