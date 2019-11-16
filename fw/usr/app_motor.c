/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "app_main.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

static gpio_t drv_en = { .group = DRV_EN_GPIO_Port, .num = DRV_EN_Pin };
static gpio_t drv_md1 = { .group = DRV_MD1_GPIO_Port, .num = DRV_MD1_Pin };
static gpio_t drv_md2 = { .group = DRV_MD2_GPIO_Port, .num = DRV_MD2_Pin };
static gpio_t drv_md3 = { .group = DRV_MD3_GPIO_Port, .num = DRV_MD3_Pin };
static gpio_t drv_step = { .group = DRV_STEP_GPIO_Port, .num = DRV_STEP_Pin };
static gpio_t drv_dir = { .group = DRV_DIR_GPIO_Port, .num = DRV_DIR_Pin };

gpio_t limit_det = { .group = LIMIT_DET_GPIO_Port, .num = LIMIT_DET_Pin };

static cdnet_socket_t pos_sock = { .port = 20 };
static cdnet_socket_t speed_sock = { .port = 21 };


// 200 step per 360 'C
#define INIT_PERIOD 1000

static int min_accel = 1;
static int max_accel = 1000;
static int min_period = 50;
static int max_period = INIT_PERIOD;
static int max_pos = 130000UL;

static int cur_accel = 1;

static int tgt_pos = 0;
static int tgt_period = INIT_PERIOD;
static int end_period = INIT_PERIOD;

static int cur_pos = 0;
static int cur_period = INIT_PERIOD;

#define CMD_MAX 10
static cmd_t cmd_alloc[CMD_MAX];
static list_head_t cmd_free_head = {0};
static list_head_t cmd_pend_head = {0};
static state_t state = ST_OFF;
static uint32_t wait_until = 0;


static void goto_pos(int pos, int period, int accel)
{
    tgt_pos = min(pos, max_pos);
    gpio_set_value(&drv_dir, tgt_pos >= cur_pos); // 0: -, 1: +
    //cur_period = max_period;
    tgt_period = clip(period, min_period, max_period);
    cur_accel = clip(accel, min_accel, max_accel);
    if (tgt_pos == cur_pos)
        return;
    state = ST_RUN;
    __HAL_TIM_SET_AUTORELOAD(&htim1, cur_period);
    __HAL_TIM_ENABLE(&htim1);
}


void app_motor_init(void)
{
    int i;
    for (i = 0; i < CMD_MAX; i++)
        list_put(&cmd_free_head, &cmd_alloc[i].node);

    cdnet_socket_bind(&pos_sock, NULL);
    cdnet_socket_bind(&speed_sock, NULL);

    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    set_led_state(LED_POWERON);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 300);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);

    gpio_set_value(&drv_md1, 0);
    gpio_set_value(&drv_md2, 0);
    gpio_set_value(&drv_md3, 1);
    gpio_set_value(&drv_en, 1);
    state = ST_STOP;

    goto_pos(max_pos * -2, 200, 3); // go home
}


static void pos_mode_service_routine(void)
{
    cdnet_packet_t *pkt = cdnet_socket_recvfrom(&pos_sock);
    if (!pkt)
        return;

    if (pkt->len != 17 || pkt->dat[0] != 0x60) {
        d_warn("pos mode: wrong len: %d\n", pkt->len);
        list_put(&cdnet_free_pkts, &pkt->node);
        return;
    }
    cmd_t *cmd = list_get_entry(&cmd_free_head, cmd_t);
    if (cmd) {
        cmd->pos = *(int *)(pkt->dat + 1);
        cmd->period = 1000000 / *(int *)(pkt->dat + 5);
        cmd->accel = *(int *)(pkt->dat + 9);
        cmd->time = *(int *)(pkt->dat + 13);
        list_put(&cmd_pend_head, &cmd->node);
        d_debug("pos mode: %d %d(%d) %d %d (len: %d)\n",
                cmd->pos, *(int *)(pkt->dat + 5), cmd->period,
                cmd->accel, cmd->time, cmd_pend_head.len);
    }
    pkt->len = 2;
    pkt->dat[0] = cmd ? 0x80 : 0x81;
    pkt->dat[1] = cmd_pend_head.len;

    pkt->dst = pkt->src;
    cdnet_socket_sendto(&pos_sock, pkt);
}

static void speed_mode_service_routine(void)
{
    cdnet_packet_t *pkt = cdnet_socket_recvfrom(&speed_sock);
    if (!pkt)
        return;

    if (pkt->dat[0] == 0x20 || pkt->dat[0] == 0x21) {
        int speed = *(int *)(pkt->dat + 1);
        if (pkt->dat[0] == 0x21) {
            cur_accel = clip(*(int *)(pkt->dat + 5), min_accel, max_accel);
            d_debug("speed mode: %d %d\n", speed, cur_accel);
        } else {
            d_debug("speed mode: %d\n", speed);
        }

        if (speed == 0 || gpio_get_value(&drv_dir) != (speed >= 0)) {
            if (state == ST_RUN) {
                tgt_period = -1; // stop now
                d_debug("speed mode: wait stop\n");
                while (state == ST_RUN);
            }
        }

        if (speed != 0) {
            int pos = speed > 0 ? max_pos : 0;
            int period = clip(1000000 / abs(speed), min_period, max_period);
            local_irq_disable();
            if (state == ST_RUN) {
                tgt_period = period;
                tgt_pos = pos;
            } else {
                goto_pos(pos, period, cur_accel);
            }
            local_irq_enable();
        }

        while (cmd_pend_head.len)
            list_put_begin(&cmd_free_head, list_get(&cmd_pend_head));
    }
    list_put(&cdnet_free_pkts, &pkt->node);
}


void app_motor(void)
{
    pos_mode_service_routine();
    speed_mode_service_routine();

    if (state == ST_WAIT) {
        if (get_systick() - wait_until >= 0) {
            state = ST_STOP;
        }
    } else if (state == ST_STOP && cmd_pend_head.len) {
        cmd_t *cmd = list_get_entry(&cmd_pend_head, cmd_t);
        if (cmd->time) {
            state = ST_WAIT;
            wait_until = get_systick() + cmd->time;
        } else {
            goto_pos(cmd->pos, cmd->period, cmd->accel);
        }
        list_put(&cmd_free_head, &cmd->node);

    } else if (state == ST_RUN) {
        cmd_t *cmd = list_entry_safe(cmd_pend_head.first, cmd_t);
        if (cmd && !cmd->time &&
                (gpio_get_value(&drv_dir) == (cmd->pos >= tgt_pos)))
            end_period = clip(cmd->period, min_period, max_period);
        else
            end_period = INIT_PERIOD;
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    //d_debug("tim: t: %ld, cur_pos: %d, cur_period: %d\n",
    //        get_systick(), cur_pos, cur_period);
    gpio_get_value(&drv_dir) ? cur_pos++ : cur_pos--;

    if (cur_pos == tgt_pos) {
        d_debug("tim: arrive pos %d, period: %d\n", tgt_pos, cur_period);
        state = ST_STOP;
        return;
    }

    if (tgt_period == -1) {
        cur_period += min(abs(max_period - cur_period), cur_accel);
        if (cur_period >= max_period) {
            d_debug("tim: early stop at %d, target: %d\n", cur_pos, tgt_pos);
            state = ST_STOP;
            return;
        }
    } else if (abs(end_period - cur_period) / cur_accel + 1 >= abs(tgt_pos - cur_pos)) {
        cur_period += sign(end_period - cur_period) *
                min(abs(end_period - cur_period), cur_accel);
    } else {
        cur_period -= sign(cur_period - tgt_period) *
                min(abs(cur_period - tgt_period), cur_accel);
    }

    __HAL_TIM_SET_AUTORELOAD(&htim1, cur_period);
    __HAL_TIM_ENABLE(&htim1);

    gpio_set_value(&drv_step, 1);
    gpio_set_value(&drv_step, 0);
}

void limit_det_isr(void)
{
    d_debug("lim: detected\n");
    state = ST_STOP;
    __HAL_TIM_DISABLE(&htim1);
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    cur_period = max_period;
    cur_pos = -1000;
    goto_pos(0, 50, 3);
}
