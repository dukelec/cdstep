/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "math.h"
#include "app_main.h"

extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

int CDCTL_SYS_CLK = 150000000; // 150MHz for cdctl01a

gpio_t led_r = { .group = LED_R_GPIO_Port, .num = LED_R_Pin };
gpio_t led_g = { .group = LED_G_GPIO_Port, .num = LED_G_Pin };

uart_t debug_uart = { .huart = &huart1 };

static gpio_t r_int = { .group = CD_INT_GPIO_Port, .num = CD_INT_Pin };
static gpio_t r_cs = { .group = CD_CS_GPIO_Port, .num = CD_CS_Pin };
static spi_t r_spi = { .hspi = &hspi1, .ns_pin = &r_cs };

static gpio_t sen_int = { .group = SEN_INT_GPIO_Port, .num = SEN_INT_Pin }; // position limit

static cd_frame_t frame_alloc[FRAME_MAX];
list_head_t frame_free_head = {0};

static cdn_pkt_t packet_alloc[PACKET_MAX];
list_head_t packet_free_head = {0};

static cdctl_dev_t r_dev = {0};    // CDBUS
cdn_ns_t dft_ns = {0};             // CDNET


static void device_init(void)
{
    int i;
    cdn_init_ns(&dft_ns, &packet_free_head);

    for (i = 0; i < FRAME_MAX; i++)
        list_put(&frame_free_head, &frame_alloc[i].node);
    for (i = 0; i < PACKET_MAX; i++)
        list_put(&packet_free_head, &packet_alloc[i].node);

    cdctl_dev_init(&r_dev, &frame_free_head, &csa.bus_cfg, &r_spi, NULL, &r_int);

    if (r_dev.version >= 0x10) {
        // 16MHz / (2 + 2) * (73 + 2) / 2^1 = 150MHz
        cdctl_write_reg(&r_dev, REG_PLL_N, 0x2);
        d_info("pll_n: %02x\n", cdctl_read_reg(&r_dev, REG_PLL_N));
        cdctl_write_reg(&r_dev, REG_PLL_ML, 0x49); // 0x49: 73
        d_info("pll_ml: %02x\n", cdctl_read_reg(&r_dev, REG_PLL_ML));

        d_info("pll_ctrl: %02x\n", cdctl_read_reg(&r_dev, REG_PLL_CTRL));
        cdctl_write_reg(&r_dev, REG_PLL_CTRL, 0x10); // enable pll
        d_info("clk_status: %02x\n", cdctl_read_reg(&r_dev, REG_CLK_STATUS));
        cdctl_write_reg(&r_dev, REG_CLK_CTRL, 0x01); // select pll

        d_info("clk_status after select pll: %02x\n", cdctl_read_reg(&r_dev, REG_CLK_STATUS));
        d_info("version after select pll: %02x\n", cdctl_read_reg(&r_dev, REG_VERSION));
    } else {
        d_info("fallback to cdctl-b1 module, ver: %02x\n", r_dev.version);
        CDCTL_SYS_CLK = 40000000; // 40MHz
        cdctl_set_baud_rate(&r_dev, csa.bus_cfg.baud_l, csa.bus_cfg.baud_h);
    }

    cdn_add_intf(&dft_ns, &r_dev.cd_dev, csa.bus_net, csa.bus_cfg.mac);
}


extern uint32_t end; // end of bss
#define STACK_CHECK_SKIP 0x200
#define STACK_CHECK_SIZE (64 + STACK_CHECK_SKIP)

static void stack_check_init(void)
{
    int i;
    printf("stack_check_init: skip: %p ~ %p, to %p\n",
            &end, &end + STACK_CHECK_SKIP, &end + STACK_CHECK_SIZE);
    for (i = STACK_CHECK_SKIP; i < STACK_CHECK_SIZE; i+=4)
        *(uint32_t *)(&end + i) = 0xababcdcd;
}

static void stack_check(void)
{
    int i;
    for (i = STACK_CHECK_SKIP; i < STACK_CHECK_SIZE; i+=4) {
        if (*(uint32_t *)(&end + i) != 0xababcdcd) {
            printf("stack overflow %p (skip: %p ~ %p): %08lx\n",
                    &end + i, &end, &end + STACK_CHECK_SKIP, *(uint32_t *)(&end + i));
            d_error("stack overflow %p (skip: %p ~ %p): %08lx\n",
                    &end + i, &end, &end + STACK_CHECK_SKIP, *(uint32_t *)(&end + i));
            while (true);
        }
    }
}

#if 0
static void dump_hw_status(void)
{
    static int t_l = 0;
    if (get_systick() - t_l > 8000) {
        t_l = get_systick();

        d_debug("ctl: state %d, t_len %d, r_len %d, irq %d\n",
                r_dev.state, r_dev.tx_head.len, r_dev.rx_head.len,
                !gpio_get_value(r_dev.int_n));
        d_debug("  r_cnt %d (lost %d, err %d, no-free %d), t_cnt %d (cd %d, err %d)\n",
                r_dev.rx_cnt, r_dev.rx_lost_cnt, r_dev.rx_error_cnt,
                r_dev.rx_no_free_node_cnt,
                r_dev.tx_cnt, r_dev.tx_cd_cnt, r_dev.tx_error_cnt);
    }
}
#endif

void app_main(void)
{
    printf("\nstart app_main (mdrv-step)...\n");
    stack_check_init();
    load_conf();
    debug_init(&dft_ns, &csa.dbg_dst, &csa.dbg_en);
    device_init();
    common_service_init();
    raw_dbg_init();
    printf("conf (mdrv-step): %s\n", csa.conf_from ? "load from flash" : "use default");
    d_info("conf (mdrv-step): %s\n", csa.conf_from ? "load from flash" : "use default");
    d_info("\x1b[92mColor Test\x1b[0m and \x1b[93mAnother Color\x1b[0m...\n");
    csa_list_show();
    app_motor_init();

    gpio_set_value(&led_g, 1);
    //uint32_t t_last = get_systick();
    while (true) {
        //if (get_systick() - t_last > (gpio_get_value(&led_g) ? 400 : 600)) {
        //    t_last = get_systick();
        //    gpio_set_value(&led_g, !gpio_get_value(&led_g));
        //}
        stack_check();
        //dump_hw_status();
        app_motor_routine();
        cdn_routine(&dft_ns); // handle cdnet
        common_service_routine();
        raw_dbg_routine();
        debug_flush(false);
    }
}


void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == r_int.num) {
        cdctl_int_isr(&r_dev);
    } else if (GPIO_Pin == sen_int.num) {
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
    printf("spi error... [%08lx]\n", hspi->ErrorCode);
}
