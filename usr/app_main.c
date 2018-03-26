/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "app_main.h"
#include "cdctl_bx_it.h"
#include "modbus_crc.h"
#include "main.h"

extern UART_HandleTypeDef huart2;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;

static gpio_t led_r = { .group = LED_R_GPIO_Port, .num = LED_R_Pin };
static gpio_t led_g = { .group = LED_G_GPIO_Port, .num = LED_G_Pin };
static gpio_t led_b = { .group = LED_B_GPIO_Port, .num = LED_B_Pin };

uart_t debug_uart = { .huart = &huart2 };

static gpio_t drv_en = { .group = DRV_EN_GPIO_Port, .num = DRV_EN_Pin };
static gpio_t drv_md1 = { .group = DRV_MD1_GPIO_Port, .num = DRV_MD1_Pin };
static gpio_t drv_md2 = { .group = DRV_MD2_GPIO_Port, .num = DRV_MD2_Pin };
static gpio_t drv_md3 = { .group = DRV_MD3_GPIO_Port, .num = DRV_MD3_Pin };
static gpio_t drv_step = { .group = DRV_STEP_GPIO_Port, .num = DRV_STEP_Pin };
static gpio_t drv_dir = { .group = DRV_DIR_GPIO_Port, .num = DRV_DIR_Pin };

static gpio_t pwr_det = { .group = PWR_DET_GPIO_Port, .num = PWR_DET_Pin };
static gpio_t limit_det = { .group = LIMIT_DET_GPIO_Port, .num = LIMIT_DET_Pin };

static gpio_t r_rst_n = { .group = CDCTL_RST_N_GPIO_Port, .num = CDCTL_RST_N_Pin };
static gpio_t r_int_n = { .group = CDCTL_INT_N_GPIO_Port, .num = CDCTL_INT_N_Pin };
static gpio_t r_ns = { .group = CDCTL_NS_GPIO_Port, .num = CDCTL_NS_Pin };
static spi_t r_spi = { .hspi = &hspi2, .ns_pin = &r_ns };

#define FRAME_MAX 10
static cd_frame_t frame_alloc[FRAME_MAX];
static list_head_t frame_free_head = {0};

#define PACKET_MAX 10
static cdnet_packet_t packet_alloc[PACKET_MAX];
static list_head_t packet_free_head = {0};

cdctl_intf_t r_intf = {0};   // RS485
cdnet_intf_t n_intf = {0};   // CDNET


static void device_init(void)
{
    int i;
    for (i = 0; i < FRAME_MAX; i++)
        list_put(&frame_free_head, &frame_alloc[i].node);
    for (i = 0; i < PACKET_MAX; i++)
        list_put(&packet_free_head, &packet_alloc[i].node);

    cdctl_intf_init(&r_intf, &frame_free_head, app_conf.rs485_addr.mac,
            app_conf.rs485_baudrate_low, app_conf.rs485_baudrate_high,
            &r_spi, &r_rst_n, &r_int_n);
    cdnet_intf_init(&n_intf, &packet_free_head, &r_intf.cd_intf, &app_conf.rs485_addr);
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

extern uint32_t end; // end of bss


void app_main(void)
{
    debug_init();
    device_init();
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    d_debug("start app_main...\n");
    *(uint32_t *)(&end) = 0xababcdcd;
    set_led_state(LED_POWERON);

    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 300);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);

    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
    __HAL_TIM_SET_AUTORELOAD(&htim1, 65530);
    __HAL_TIM_ENABLE(&htim1);

    gpio_set_value(&drv_md1, 0);
    gpio_set_value(&drv_md2, 1);
    gpio_set_value(&drv_md3, 0);
    gpio_set_value(&drv_en, 1);
    gpio_set_value(&drv_dir, 1);

    while (true) {

        if (*(uint32_t *)(&end) != 0xababcdcd) {
            printf("stack overflow: %08lx\n", *(uint32_t *)(&end));
            while (true);
        }

        {
            static int t_l = 0;
            if (get_systick() - t_l > 200) {
                t_l = get_systick();
                d_debug("... %ld\n", t_l);
                __HAL_TIM_ENABLE(&htim1);

                gpio_set_value(&drv_step, 1);
                gpio_set_value(&drv_step, 0);
            }
        }

        // handle cdnet

        cdnet_rx(&n_intf);
        cdnet_packet_t *pkt = cdnet_packet_get(&n_intf.rx_head);
        if (pkt) {
            if (pkt->level == CDNET_L2 || pkt->src_port < CDNET_DEF_PORT ||
                    pkt->dst_port >= CDNET_DEF_PORT) {
                d_warn("unexpected pkg port\n");
                list_put(n_intf.free_head, &pkt->node);
            } else {
                switch (pkt->dst_port) {
                case 1:
                    p1_service(pkt);
                    break;
                case 2:
                    p2_service(pkt);
                    break;
                case 3:
                    p3_service(pkt);
                    break;

                case 15:
                    if (pkt->len == 4) {
                        /*
                        app_conf.rpt_en = !!(pkt->dat[0] & 0x80);
                        app_conf.rpt_multi = pkt->dat[0] & 0x30;
                        app_conf.rpt_seq = pkt->dat[0] & 0x08;
                        app_conf.rpt_pkt_level = pkt->dat[0] & 0x03;
                        app_conf.rpt_mac = pkt->dat[1];
                        app_conf.rpt_addr.net = pkt->dat[2];
                        app_conf.rpt_addr.mac = pkt->dat[3];
                        */

                        pkt->len = 0;
                        cdnet_exchg_src_dst(&n_intf, pkt);
                        list_put(&n_intf.tx_head, &pkt->node);
                        d_debug("raw_conf: ...\n");
                    } else {
                        // TODO: report setting
                        list_put(n_intf.free_head, &pkt->node);
                        d_warn("raw_conf: wrong len: %d\n", pkt->len);
                    }
                    break;

                default:
                    d_warn("unknown pkg\n");
                    list_put(n_intf.free_head, &pkt->node);
                }
            }

        }
        cdnet_tx(&n_intf);

        debug_flush();
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    d_debug("timer call back %ld\n", get_systick());
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == r_int_n.num) {
        cdctl_int_isr(&r_intf);
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    cdctl_spi_isr(&r_intf);
}
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    cdctl_spi_isr(&r_intf);
}
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    cdctl_spi_isr(&r_intf);
}
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    d_error("spi error...\n");
}
