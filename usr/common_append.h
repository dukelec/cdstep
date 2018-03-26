/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __COMMON_APPEND_H__
#define __COMMON_APPEND_H__

typedef enum {
    LED_POWERON = 0,
    LED_WARN,
    LED_ERROR
} led_state_t;

void set_led_state(led_state_t state);


#undef d_warn
#undef d_error
#undef dd_warn
#undef dd_error

#define d_warn(fmt, ...) do {               \
        set_led_state(LED_WARN);            \
        dprintf("W: " fmt, ## __VA_ARGS__); \
    } while (0)

#define d_error(fmt, ...) do {              \
        set_led_state(LED_ERROR);           \
        dprintf("E: " fmt, ## __VA_ARGS__); \
    } while (0)

#define dd_warn(name, fmt, ...) do {                    \
        set_led_state(LED_WARN);                        \
        dprintf("W: %s: " fmt, name, ## __VA_ARGS__);   \
    } while (0)

#define dd_error(name, fmt, ...) do {                   \
        set_led_state(LED_ERROR);                        \
        dprintf("E: %s: " fmt, name, ## __VA_ARGS__);   \
    } while (0)


static inline
void dbg_transmit(uart_t *uart, const uint8_t *buf, uint16_t len)
{
#if 1 // avoid hal check
    uint16_t i;
    for (i = 0; i < len; i++) {
        while (!__HAL_UART_GET_FLAG(uart->huart, UART_FLAG_TXE));
        uart->huart->Instance->DR = *(buf + i);
    }
#else
    HAL_UART_Transmit(uart->huart, (uint8_t *)buf, len, HAL_MAX_DELAY);
#endif
}

static inline
void dbg_transmit_it(uart_t *uart, const uint8_t *buf, uint16_t len)
{
    HAL_UART_Transmit_DMA(uart->huart, (uint8_t *)buf, len);
}

static inline bool dbg_transmit_is_ready(uart_t *uart)
{
#if 1 // DMA
    if (uart->huart->TxXferCount == 0) {
        uart->huart->gState = HAL_UART_STATE_READY;
        return true;
    } else {
        return false;
    }
#else
    return uart->huart->gState == HAL_UART_STATE_READY;
#endif
}

#endif
