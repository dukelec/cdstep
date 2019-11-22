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
    tgt_period = clip(period, min_period, max_period);
    cur_accel = clip(accel, min_accel, max_accel);

    if (tgt_pos != cur_pos && state != ST_RUN) {
        state = ST_RUN;
        __HAL_TIM_SET_AUTORELOAD(&htim1, cur_period);
        __HAL_TIM_ENABLE(&htim1);
    }
}


void app_motor_init(void)
{
    int i;
    for (i = 0; i < CMD_MAX; i++)
        list_put(&cmd_free_head, &cmd_alloc[i].node);

    cdnet_socket_bind(&pos_sock, NULL);

    //HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    set_led_state(LED_POWERON);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 450);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);

    gpio_set_value(&drv_md1, 0);
    gpio_set_value(&drv_md2, 0);
    gpio_set_value(&drv_md3, 1);
    gpio_set_value(&drv_en, 1);
    state = ST_STOP;

    //goto_pos(max_pos * -2, 200, 3); // go home
}

// 0x00: return: pos, speed ...
// 0x20: pos, speed, accel  (realtime)
// 0x21: pos, speed, accel  (cmd list)
// 0x22: sleep              (cmd list)

static void pos_mode_service_routine(void)
{
    cdnet_packet_t *pkt = cdnet_socket_recvfrom(&pos_sock);
    if (!pkt)
        return;

    if (pkt->dat[0] != 0x21 && pkt->dat[0] != 0x22) {
        d_warn("pos mode: wrong cmd\n");
        list_put(&cdnet_free_pkts, &pkt->node);
        return;
    }

    cmd_t *cmd = list_get_entry(&cmd_free_head, cmd_t);
    if (cmd) {
        if (pkt->dat[0] == 0x21) {
            cmd->pos = *(int *)(pkt->dat + 1);
            cmd->period = 1000000 / *(int *)(pkt->dat + 5);
            cmd->accel = *(int *)(pkt->dat + 9);
            cmd->time = 0;
            d_debug("pos add: %d %d(%d) %d (len: %d)\n",
                    cmd->pos, *(int *)(pkt->dat + 5), cmd->period,
                    cmd->accel, cmd_pend_head.len);
        } else {
            cmd->time = *(int *)(pkt->dat + 1);
            d_debug("pos add time: %d (len: %d)\n", cmd->time, cmd_pend_head.len);
        }
        list_put_it(&cmd_pend_head, &cmd->node);
    }
    pkt->len = 2;
    pkt->dat[0] = cmd ? 0x80 : 0x81;
    pkt->dat[1] = cmd_pend_head.len;

    pkt->dst = pkt->src;
    cdnet_socket_sendto(&pos_sock, pkt);
}

void app_motor(void)
{
    pos_mode_service_routine();

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

    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    //d_debug("tim: t: %ld, cur_pos: %d, cur_period: %d\n",
    //        get_systick(), cur_pos, cur_period);
    gpio_get_value(&drv_dir) ? cur_pos++ : cur_pos--;

    if (cur_pos == tgt_pos) {
        cmd_t *cmd = list_entry_safe(cmd_pend_head.first, cmd_t);
        d_debug("tim: arrive pos %d, period: %d\n", tgt_pos, cur_period);
        if (cmd && !cmd->time) {
            goto_pos(cmd->pos, cmd->period, cmd->accel);
            list_put(&cmd_free_head, list_get(&cmd_pend_head));
        } else {
            cur_period = INIT_PERIOD;
            state = ST_STOP;
            return;
        }
    }

    {
        cmd_t *cmd = list_entry_safe(cmd_pend_head.first, cmd_t);
        if (cmd && !cmd->time && (gpio_get_value(&drv_dir) == (cmd->pos >= tgt_pos)))
            end_period = clip(cmd->period, min_period, max_period);
        else
            end_period = INIT_PERIOD;
    }

    if (tgt_period == -1) {
        cur_period += min(abs(max_period - cur_period), cur_accel);
        if (cur_period >= max_period) {
            d_debug("tim: early stop at %d, target: %d\n", cur_pos, tgt_pos);
            cur_period = INIT_PERIOD;
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
    //goto_pos(0, 50, 3);
}
