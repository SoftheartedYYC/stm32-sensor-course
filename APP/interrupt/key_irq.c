/**
 * @file    key_irq.c
 * @brief   四个独立按键（中断版）：EXTI 捕获边沿 + 1ms 时基做消抖和事件判定
 *
 * 事件判定流程（每个键独立）：
 *   1. EXTI 双边沿：只记"有边沿、时间是多少"，不判断；
 *   2. 1ms 钩子里，边沿过去 DEBOUNCE_MS(20ms) 后读一次引脚：
 *        低 -> 确认按下，开始计时；
 *        高 -> 确认松开，根据按住时长和"上一次是否在等双击"决定发什么事件；
 *   3. 按住满 LONG_MS(500ms)：立刻发 KEY_EVENT_LONG，之后每 REPEAT_MS(100ms) 发连发；
 *   4. 松开后等 DOUBLE_MS(300ms)：没有第二下 -> 发 KEY_EVENT_SHORT；
 *      300ms 内有第二下 -> 第二次松开时发 KEY_EVENT_DOUBLE。
 *
 * 注意：因为要等双击窗口，短按事件会比松开晚 300ms 左右；长按是立刻发的。
 */

#include "key_irq.h"
#include "tick_irq.h"
#include "soft_timer.h"

#define KEY_NUM        4
#define KEY_SCAN_MS    1u  /* 按键状态机周期：1ms 扫一次 */
#define KEY_QUEUE_SIZE 8

static const uint16_t s_key_pins[KEY_NUM] = {
    KEYIRQ_PIN0, KEYIRQ_PIN1, KEYIRQ_PIN2, KEYIRQ_PIN3};

static const uint32_t s_exti_lines[KEY_NUM] = {
    EXTI_Line0, EXTI_Line1, EXTI_Line2, EXTI_Line3};

static volatile uint8_t  s_down[KEY_NUM];        /* 1 = 已确认按下 */
static volatile uint8_t  s_pending[KEY_NUM];     /* 1 = 有边沿，等 20ms 确认 */
static volatile uint32_t s_edge_ms[KEY_NUM];     /* 最近一次边沿的时间 */
static volatile uint32_t s_press_ms[KEY_NUM];    /* 本次按下的边沿时间 */
static volatile uint32_t s_release_ms[KEY_NUM];  /* 本次松开的边沿时间 */
static volatile uint32_t s_repeat_ms[KEY_NUM];   /* 下一次连发的时间 */
static volatile uint8_t  s_long_sent[KEY_NUM];   /* 1 = 这次长按已经报过 */
static volatile uint8_t  s_second[KEY_NUM];      /* 1 = 这次按下是双击的第二下 */
static volatile uint8_t  s_wait_single[KEY_NUM]; /* 1 = 松开后正在等双击窗口 */

static soft_timer_t s_key_timer;                 /* 软定时器：每 1ms 跑一次状态机 */

static volatile uint8_t s_queue[KEY_QUEUE_SIZE]; /* 事件队列，中断写、主循环读 */
static volatile uint8_t s_head = 0;
static volatile uint8_t s_tail = 0;

/**
 * @brief  把一个事件压进队列（满了就丢弃新事件）
 */
static void key_push(uint8_t ev)
{
    uint8_t next = (uint8_t)((s_head + 1) & (KEY_QUEUE_SIZE - 1));

    if (next != s_tail)
    {
        s_queue[s_head] = ev;
        s_head = next;
    }
}

/**
 * @brief  1ms 钩子：消抖确认 + 短按/长按/双击/连发判定
 */
static void key_irq_tick(void)
{
    uint8_t i;
    uint32_t now = TICK_GetMs();

    for (i = 0; i < KEY_NUM; i++)
    {
        uint32_t edge = s_edge_ms[i];

        /* ---- 1. 处理待确认的边沿 ---- */
        if ((s_pending[i] != 0) &&
            ((uint32_t)(now - edge) >= KEYIRQ_DEBOUNCE_MS))
        {
            s_pending[i] = 0;

            if (GPIO_ReadInputDataBit(KEYIRQ_PORT, s_key_pins[i]) == Bit_RESET)
            {
                /* 确认按下 */
                if (s_down[i] == 0)
                {
                    s_down[i] = 1;
                    s_press_ms[i] = edge;
                    s_long_sent[i] = 0;

                    if (s_wait_single[i] != 0)
                    {
                        /* 上一击还在等双击窗口：这一下算不算第二击？ */
                        if ((uint32_t)(edge - s_release_ms[i]) <= KEYIRQ_DOUBLE_MS)
                        {
                            s_second[i] = 1;
                        }
                        else
                        {
                            key_push((uint8_t)(KEY_EVENT_SHORT | (KEY_ID_0 + i)));
                            s_second[i] = 0;
                        }
                        s_wait_single[i] = 0;
                    }
                    else
                    {
                        s_second[i] = 0;
                    }
                }
            }
            else
            {
                /* 确认松开 */
                if (s_down[i] != 0)
                {
                    uint32_t held = (uint32_t)(edge - s_press_ms[i]);
                    s_down[i] = 0;

                    if (s_long_sent[i] != 0)
                    {
                        s_long_sent[i] = 0; /* 长按报过了，松开不再产生事件 */
                        s_second[i] = 0;
                    }
                    else if (s_second[i] != 0)
                    {
                        key_push((uint8_t)(KEY_EVENT_DOUBLE | (KEY_ID_0 + i)));
                        s_second[i] = 0;
                    }
                    else if (held >= KEYIRQ_LONG_MS)
                    {
                        /* 松开边沿来得太快、没来得及在 500ms 报长按：这里补上 */
                        key_push((uint8_t)(KEY_EVENT_LONG | (KEY_ID_0 + i)));
                    }
                    else
                    {
                        s_wait_single[i] = 1;
                        s_release_ms[i] = edge;
                    }
                }
            }
        }

        /* ---- 2. 长按：按住满 500ms，立刻报一次 ---- */
        if ((s_down[i] != 0) && (s_long_sent[i] == 0) && (s_pending[i] == 0) &&
            ((uint32_t)(now - s_press_ms[i]) >= KEYIRQ_LONG_MS))
        {
            s_long_sent[i] = 1;
            s_second[i] = 0; /* 按得久了就不算双击的第二下 */
            s_repeat_ms[i] = now + KEYIRQ_REPEAT_MS;
            key_push((uint8_t)(KEY_EVENT_LONG | (KEY_ID_0 + i)));
        }

        /* ---- 3. 长按后的连发 ---- */
        if ((s_down[i] != 0) && (s_long_sent[i] != 0) && (s_pending[i] == 0) &&
            ((int32_t)(now - s_repeat_ms[i]) >= 0))
        {
            s_repeat_ms[i] += KEYIRQ_REPEAT_MS;
            key_push((uint8_t)(KEY_EVENT_REPEAT | (KEY_ID_0 + i)));
        }

        /* ---- 4. 短按：松开后等满双击窗口，没有第二下才算 ---- */
        if ((s_wait_single[i] != 0) && (s_pending[i] == 0) &&
            ((uint32_t)(now - s_release_ms[i]) >= KEYIRQ_DOUBLE_MS))
        {
            s_wait_single[i] = 0;
            key_push((uint8_t)(KEY_EVENT_SHORT | (KEY_ID_0 + i)));
        }

        /* ---- 5. 兜底：确认按下却读到高（漏了松开边沿），补一次待确认 ---- */
        if ((s_down[i] != 0) && (s_pending[i] == 0) &&
            (GPIO_ReadInputDataBit(KEYIRQ_PORT, s_key_pins[i]) != Bit_RESET))
        {
            s_pending[i] = 1;
            s_edge_ms[i] = now;
        }
    }
}

/**
 * @brief  EXTI 公共处理：清标志、记时间和"有待确认边沿"
 */
static void key_irq_exti(uint8_t idx)
{
    if (EXTI_GetITStatus(s_exti_lines[idx]) != RESET)
    {
        EXTI_ClearITPendingBit(s_exti_lines[idx]);

        s_edge_ms[idx] = TICK_GetMs();
        s_pending[idx] = 1;
    }
}

/* 四个 EXTI 各自一个中断入口（中断名固定，只能定义一次） */
void EXTI0_IRQHandler(void)
{
    key_irq_exti(0);
}

void EXTI1_IRQHandler(void)
{
    key_irq_exti(1);
}

void EXTI2_IRQHandler(void)
{
    key_irq_exti(2);
}

void EXTI3_IRQHandler(void)
{
    key_irq_exti(3);
}

/**
 * @brief  初始化按键中断
 * @note   先调 SOFTTIMER_Init()/TICK_Init()，本函数会注册 1ms 的软定时任务；
 *         最后在主程序里调 TICK_Start() 启动时基
 */
void KeyIRQ_Init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;
    uint8_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO, ENABLE);

    /* 四个键：上拉输入，松开 = 高、按下 = 低 */
    gpio.GPIO_Pin = KEYIRQ_PIN0 | KEYIRQ_PIN1 | KEYIRQ_PIN2 | KEYIRQ_PIN3;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(KEYIRQ_PORT, &gpio);

    for (i = 0; i < KEY_NUM; i++)
    {
        s_down[i] = 0;
        s_pending[i] = 0;
        s_edge_ms[i] = 0;
        s_press_ms[i] = 0;
        s_release_ms[i] = 0;
        s_repeat_ms[i] = 0;
        s_long_sent[i] = 0;
        s_second[i] = 0;
        s_wait_single[i] = 0;
    }
    s_head = 0;
    s_tail = 0;

    for (i = 0; i < KEY_NUM; i++)
    {
        /* PE0 -> EXTI0 ... PE3 -> EXTI3 */
        GPIO_EXTILineConfig(GPIO_PortSourceGPIOE, (uint8_t)(GPIO_PinSource0 + i));

        exti.EXTI_Line = s_exti_lines[i];
        exti.EXTI_Mode = EXTI_Mode_Interrupt;
        exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling; /* 按下、松开都要捕获 */
        exti.EXTI_LineCmd = ENABLE;
        EXTI_Init(&exti);

        /* 优先级 2：和蜂鸣器同级，高于 1ms 时基(3) */
        nvic.NVIC_IRQChannel = (uint8_t)(EXTI0_IRQn + i);
        nvic.NVIC_IRQChannelPreemptionPriority = 2;
        nvic.NVIC_IRQChannelSubPriority = 0;
        nvic.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&nvic);
    }

    SOFTTIMER_Add(&s_key_timer, key_irq_tick, KEY_SCAN_MS);
}

/**
 * @brief  从队列取一个按键事件
 * @retval KEY_NONE 或 (键号 | 事件类型)
 */
uint8_t KeyIRQ_Scan(void)
{
    uint8_t ev;

    if (s_tail == s_head)
    {
        return KEY_NONE;
    }

    ev = s_queue[s_tail];
    s_tail = (uint8_t)((s_tail + 1) & (KEY_QUEUE_SIZE - 1));
    return ev;
}
