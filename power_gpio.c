#include <stdint.h>
#include "power_gpio.h"
#include "net.h"
#include "uart.h"
#include "st/stm32f10x.h"

static uint8_t pwr_mask = 0x0f; /* all on by default */

void set_power_pin_state(void)
{
    GPIOA->CRL |= (1 << 1) | (1 << 5);
    GPIOA->CRL &= ~((1 << 0) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7));
    GPIOB->CRL |= (1 << 1) | (1 << 5);
    GPIOB->CRL &= ~((1 << 0) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7));
    GPIOA->ODR |= 3;
    GPIOB->ODR |= 3;
}

void set_power(void *p_msg, uint16_t msg_len)
{
    uint16_t pack_size;
    msg_t *msg = (msg_t*)p_msg;

    if (msg_len != sizeof(msg_t)) {
        uart_printf("got wrong payload size %d!\r\n", msg_len);
        return;
    }
    if (0 != memcmp(msg->sig, "MUMU", 4)) {
        uart_puts("Invalid sig!\r\n");
        return;
    } else {
#if NET_DEBUG
        uart_puts("received our msg!\r\n");
#endif
    }
#if NET_DEBUG
    uart_printf("msg ver %02d mask %02x\r\n", msg->ver, msg->set_clear_mask);
#endif
    if (*(uint32_t*)&udp_peer_ip6[0] == 0) {
        uart_puts("No ipv6 udp peer set!\r\n");
        return;
    }
    pwr_mask |= (msg->set_clear_mask >> 4);
    pwr_mask &= ~(msg->set_clear_mask & 0xf);
    // Set
    /* Bit0 = PA0 */
    if (pwr_mask & 1) {
        GPIOA->ODR |= 1;
    } else {
        GPIOA->ODR &= ~1;
    }
    /* Bit1 = PA1 */
    if (pwr_mask & 2) {
        GPIOA->ODR |= 2;
    } else {
        GPIOA->ODR &= ~2;
    }
    // Clear
    /* Bit2 = PB0 */
    if (pwr_mask & 4) {
        GPIOB->ODR |= 1;
    } else {
        GPIOB->ODR &= ~1;
    }
    /* Bit0 = PA1 */
    if (pwr_mask & 8) {
        GPIOB->ODR |= 2;
    } else {
        GPIOB->ODR &= ~2;
    }
    msg->set_clear_mask = pwr_mask;
}
