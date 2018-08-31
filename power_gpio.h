#ifndef __POWER_GPIO__
#define __POWER_GPIO__

typedef struct msg_ {
    char sig[4]; /* "MUMU" */
    uint16_t ver;
    uint8_t set_clear_mask;
    uint8_t zero;
} __attribute__((packed)) msg_t;

void set_power_pin_state(void);
void set_power(void *p_msg, uint16_t msg_len);

#endif

