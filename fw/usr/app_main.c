/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "app_main.h"

extern UART_HandleTypeDef huart2;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;

extern gpio_t limit_det;

static gpio_t led_r = { .group = LED_R_GPIO_Port, .num = LED_R_Pin };
static gpio_t led_g = { .group = LED_G_GPIO_Port, .num = LED_G_Pin };
static gpio_t led_b = { .group = LED_B_GPIO_Port, .num = LED_B_Pin };

uart_t debug_uart = { .huart = &huart2 };

static gpio_t r_rst_n = { .group = CDCTL_RST_N_GPIO_Port, .num = CDCTL_RST_N_Pin };
static gpio_t r_int_n = { .group = CDCTL_INT_N_GPIO_Port, .num = CDCTL_INT_N_Pin };
static gpio_t r_ns = { .group = CDCTL_NS_GPIO_Port, .num = CDCTL_NS_Pin };
static spi_t r_spi = { .hspi = &hspi2, .ns_pin = &r_ns };

#define FRAME_MAX 10
static cd_frame_t frame_alloc[FRAME_MAX];
static list_head_t frame_free_head = {0};

#define PACKET_MAX 10
static cdnet_packet_t packet_alloc[PACKET_MAX];

static cdctl_dev_t r_dev = {0};    // RS485
static cdnet_intf_t n_intf = {0};  // CDNET


static void device_init(void)
{
    int i;
    for (i = 0; i < FRAME_MAX; i++)
        list_put(&frame_free_head, &frame_alloc[i].node);
    for (i = 0; i < PACKET_MAX; i++)
        list_put(&cdnet_free_pkts, &packet_alloc[i].node);

    cdctl_dev_init(&r_dev, &frame_free_head, app_conf.rs485_mac,
            app_conf.rs485_baudrate_low, app_conf.rs485_baudrate_high,
            &r_spi, &r_rst_n, &r_int_n);
    cdnet_intf_init(&n_intf, &r_dev.cd_dev, app_conf.rs485_net, app_conf.rs485_mac);
}

void set_led_state(led_state_t state)
{
    static bool is_err = false;
    if (is_err)
        return;

    switch (state) {
    case LED_POWERON:
        gpio_set_value(&led_r, 1);
        gpio_set_value(&led_g, 1);
        gpio_set_value(&led_b, 0);
        break;
    case LED_WARN:
        gpio_set_value(&led_r, 0);
        gpio_set_value(&led_g, 0);
        gpio_set_value(&led_b, 1);
        break;
    default:
    case LED_ERROR:
        is_err = true;
        gpio_set_value(&led_r, 0);
        gpio_set_value(&led_g, 1);
        gpio_set_value(&led_b, 1);
        break;
    }
}

#ifdef BOOTLOADER
#define APP_ADDR 0x08010000 // offset: 64KB

static void jump_to_app(void)
{
    uint32_t stack = *(uint32_t*)APP_ADDR;
    uint32_t func = *(uint32_t*)(APP_ADDR + 4);
    printf("jump to app...\n");
    __set_MSP(stack); // init stack pointer
    ((void(*)()) func)();
}
#endif


void app_main(void)
{
#ifdef BOOTLOADER
    printf("\nstart app_main (bl_wait: %d)...\n", app_conf.bl_wait);
#else
    printf("\nstart app_main...\n");
#endif
    debug_init();
    load_conf();
    device_init();
    common_service_init();
    app_motor_init();

#ifdef BOOTLOADER
    uint32_t boot_time = get_systick();
#endif

    while (true) {
#ifdef BOOTLOADER
        if (app_conf.bl_wait != 0xff &&
                get_systick() - boot_time > app_conf.bl_wait * 100000 / SYSTICK_US_DIV)
            jump_to_app();
#endif

        cdnet_intf_routine(); // handle cdnet
        common_service_routine();
        app_motor();
        debug_flush();
    }
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == r_int_n.num) {
        cdctl_int_isr(&r_dev);
    } else if (GPIO_Pin == limit_det.num) {
        limit_det_isr();
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    cdctl_spi_isr(&r_dev);
}
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    cdctl_spi_isr(&r_dev);
}
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    cdctl_spi_isr(&r_dev);
}
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    d_error("spi error...\n");
}
