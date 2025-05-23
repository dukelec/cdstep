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

gpio_t led_r = { .group = LED_R_GPIO_Port, .num = LED_R_Pin };
gpio_t led_g = { .group = LED_G_GPIO_Port, .num = LED_G_Pin };

static gpio_t r_int = { .group = CD_INT_GPIO_Port, .num = CD_INT_Pin };
static gpio_t r_cs = { .group = CD_CS_GPIO_Port, .num = CD_CS_Pin };
static spi_t r_spi = {
        .spi = SPI1,
        .ns_pin = &r_cs,
        .dma_rx = DMA1,
        .dma_ch_rx = DMA1_Channel1,
        .dma_ch_tx = DMA1_Channel2,
        .dma_mask = (2 << 0)
};

static cd_frame_t frame_alloc[FRAME_MAX];
list_head_t frame_free_head = {0};

static cdn_pkt_t packet_alloc[PACKET_MAX];
list_head_t packet_free_head = {0};

cdctl_dev_t r_dev = {0};    // CDBUS
cdn_ns_t dft_ns = {0};      // CDNET


static void device_init(void)
{
    int i;
    cdn_init_ns(&dft_ns, &packet_free_head, &frame_free_head);

    for (i = 0; i < FRAME_MAX; i++)
        cd_list_put(&frame_free_head, &frame_alloc[i]);
    for (i = 0; i < PACKET_MAX; i++)
        cdn_list_put(&packet_free_head, &packet_alloc[i]);

    spi_wr_init(&r_spi);
    cdctl_dev_init(&r_dev, &frame_free_head, &csa.bus_cfg, &r_spi, &r_int, EXTI2_3_IRQn);

    cdn_add_intf(&dft_ns, &r_dev.cd_dev, csa.bus_net, csa.bus_cfg.mac);
}


#if 0
static void dump_hw_status(void)
{
    static int t_l = 0;
    if (get_systick() - t_l > 5000) {
        t_l = get_systick();

        d_debug("ctl: state %d, t_len %ld, r_len %ld, irq %d\n",
                r_dev.state, r_dev.tx_head.len, r_dev.rx_head.len,
                !gpio_get_val(r_dev.int_n));
        d_debug("  r_cnt %ld (lost %ld, err %ld, no-free %ld), t_cnt %ld (cd %ld, err %ld)\n",
                r_dev.rx_cnt, r_dev.rx_lost_cnt, r_dev.rx_error_cnt,
                r_dev.rx_no_free_node_cnt,
                r_dev.tx_cnt, r_dev.tx_cd_cnt, r_dev.tx_error_cnt);
    }
}
#endif

void app_main(void)
{
    uint64_t *stack_check = (uint64_t *)((uint32_t)&end + 256);
    gpio_set_val(&led_r, 1);
    gpio_set_val(&led_g, 1);

    printf("\nstart app_main (mdrv-step)...\n");
    *stack_check = 0xababcdcd12123434;
    load_conf();

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 2, 0);
    HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 2, 0);
    HAL_NVIC_SetPriority(DMA1_Ch4_7_DMAMUX1_OVR_IRQn, 2, 0);
    HAL_NVIC_SetPriority(EXTI2_3_IRQn, 2, 0);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
    HAL_NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 1, 0);

    device_init();
    common_service_init();
    d_info("conf (mdrv-step): %s\n", csa.conf_from ? "load from flash" : "use default");

    app_motor_init();
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
    HAL_NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);

    csa_list_show();
    d_info("\x1b[92mColor Test\x1b[0m and \x1b[93mAnother Color\x1b[0m...\n");
    delay_systick(100);
    gpio_set_val(&led_r, 0);
    //uint32_t t_last = get_systick();
    while (true) {
        //if (get_systick() - t_last > (gpio_get_val(&led_g) ? 400 : 600)) {
        //    t_last = get_systick();
        //    gpio_set_val(&led_g, !gpio_get_val(&led_g));
        //}
        //dump_hw_status();
        app_motor_routine();
        cdn_routine(&dft_ns); // handle cdnet
        common_service_routine();

        if (*stack_check != 0xababcdcd12123434) {
            printf("stack overflow\n");
            while (true);
        }
    }
}


void EXTI2_3_IRQHandler(void)
{
    __HAL_GPIO_EXTI_CLEAR_FALLING_IT(CD_INT_Pin);
    cdctl_int_isr(&r_dev);
}

void EXTI4_15_IRQHandler(void)
{
    __HAL_GPIO_EXTI_CLEAR_FALLING_IT(SEN_INT_Pin);
    limit_det_isr();
}

void DMA1_Channel1_IRQHandler(void)
{
    r_spi.dma_rx->IFCR = r_spi.dma_mask;
    cdctl_spi_isr(&r_dev);
}

void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
    TIM1->SR &= ~TIM_FLAG_UPDATE;
    timer_isr();
}
