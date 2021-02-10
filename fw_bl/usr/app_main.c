/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "app_main.h"

extern UART_HandleTypeDef huart2;
extern SPI_HandleTypeDef hspi2;

static gpio_t led_b = { .group = LED_B_GPIO_Port, .num = LED_B_Pin };

uart_t debug_uart = { .huart = &huart2 };

static gpio_t r_rst_n = { .group = CDCTL_RST_N_GPIO_Port, .num = CDCTL_RST_N_Pin };
static gpio_t r_ns = { .group = CDCTL_NS_GPIO_Port, .num = CDCTL_NS_Pin };
static spi_t r_spi = { .hspi = &hspi2, .ns_pin = &r_ns };

static cd_frame_t frame_alloc[FRAME_MAX];
static list_head_t frame_free_head = {0};

static cdn_pkt_t packet_alloc[PACKET_MAX];

static cdctl_dev_t r_dev = {0}; // RS485
cdn_ns_t dft_ns = {0};          // CDNET


static void device_init(void)
{
    cdn_init_ns(&dft_ns);

    for (int i = 0; i < FRAME_MAX; i++)
        list_put(&frame_free_head, &frame_alloc[i].node);
    for (int i = 0; i < PACKET_MAX; i++)
        list_put(&dft_ns.free_pkts, &packet_alloc[i].node);

    cdctl_cfg_t cfg = csa.bus_cfg;
    cfg.baud_l = cfg.baud_h = 115200;
    cdctl_dev_init(&r_dev, &frame_free_head, &cfg, &r_spi, &r_rst_n);
    cdn_add_intf(&dft_ns, &r_dev.cd_dev, csa.bus_net, csa.bus_cfg.mac);
}


#define APP_ADDR 0x08006800 // offset: 24KB

static void jump_to_app(void)
{
    uint32_t stack = *(uint32_t*)APP_ADDR;
    uint32_t func = *(uint32_t*)(APP_ADDR + 4);

    gpio_set_value(&led_b, 1);
    printf("jump to app...\n");

    // NOTE: change app's SCB->VTOR in app's system_stm32fxxx.c
    HAL_NVIC_DisableIRQ(SysTick_IRQn);
    __set_MSP(stack); // init stack pointer
    ((void(*)()) func)();
}


void app_main(void)
{
    printf("\nstart app_main (bl)...\n");
    debug_init(&dft_ns, &csa.dbg_dst, &csa.dbg_en);
    load_conf();
    device_init();
    common_service_init();
    printf("bl conf: %s\n", csa.conf_from ? "load from flash" : "use default");
    d_info("bl conf: %s\n", csa.conf_from ? "load from flash" : "use default");
    gpio_set_value(&led_b, 0);

    uint32_t t_last = get_systick();
    uint32_t boot_time = get_systick();
    bool update_baud = false;

    while (true) {
        if (get_systick() - t_last > (update_baud ? 100000 : 200000) / SYSTICK_US_DIV) {
            t_last = get_systick();
            gpio_set_value(&led_b, !gpio_get_value(&led_b));
        }

        if (!csa.keep_in_bl && !update_baud && get_systick() - boot_time > 1000000 / SYSTICK_US_DIV) {
            update_baud = true;
            if (csa.bus_cfg.baud_l != 115200 || csa.bus_cfg.baud_h != 115200) {
                cdctl_set_baud_rate(&r_dev, csa.bus_cfg.baud_l, csa.bus_cfg.baud_h);
                cdctl_flush(&r_dev);
                printf("baud rate updated, %ld %ld\n", csa.bus_cfg.baud_l, csa.bus_cfg.baud_h);
                d_info("baud rate updated, %ld %ld\n", csa.bus_cfg.baud_l, csa.bus_cfg.baud_h);
            }
        }

        if (!csa.keep_in_bl && get_systick() - boot_time > 2000000 / SYSTICK_US_DIV)
            jump_to_app();

        cdctl_routine(&r_dev);
        cdn_routine(&dft_ns); // handle cdnet
        common_service_routine();
        debug_flush(false);
    }
}

