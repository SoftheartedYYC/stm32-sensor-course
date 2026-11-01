/**
 * @file    matrix_irq.c
 * @brief   4×4 矩阵键盘（中断版）：TIM6 每 1ms 扫一行
 *
 * 扫描节奏：
 *   每 1ms 中断里只做一行：先读"上一拍拉低的那一行"的列线（已经稳定了 1ms，
 *   不需要 delay_us —— 中断里绝对不能调 delay_us，它会重配 SysTick，
 *   把主循环正在进行的 delay 搞乱），再拉低下一行。
 *   4ms 扫完 4 行；同一轮里找到的键值先记下来，每轮结束时和上一轮比较，
 *   连续 3 轮一致才认为稳定。
 */

#include "matrix_irq.h"
#include "tick_irq.h"

#define MKEY_QUEUE_SIZE 8

static uint8_t s_row;         /* 当前正在驱动的行：0~3 */
static uint8_t s_cycle_key;   /* 本轮 4 行里扫到的键值 */
static uint8_t s_stable_key;  /* 上一轮结束时的键值 */
static uint8_t s_stable_count;/* 连续相同的轮数 */
static uint8_t s_busy;        /* 1 = 这次的键已经报过了，等松手 */

static volatile uint8_t s_queue[MKEY_QUEUE_SIZE]; /* 键值队列，中断写、主循环读 */
static volatile uint8_t s_head = 0;
static volatile uint8_t s_tail = 0;

/**
 * @brief  键值入队
 */
static void matrix_push(uint8_t key)
{
    uint8_t next = (uint8_t)((s_head + 1) & (MKEY_QUEUE_SIZE - 1));

    if (next != s_tail)
    {
        s_queue[s_head] = key;
        s_head = next;
    }
}

/**
 * @brief  一轮（4ms）结束后的判决：稳定就报一次，松手后允许再报
 */
static void matrix_sample(uint8_t key)
{
    if (key == s_stable_key)
    {
        if (s_stable_count < 0xFF)
        {
            s_stable_count++;
        }
    }
    else
    {
        s_stable_key = key;
        s_stable_count = 0;
    }

    if (key == 0)
    {
        s_busy = 0; /* 松手：下一次按下可以再报 */
        return;
    }

    if ((s_busy == 0) && (s_stable_count >= MKEY_STABLE_CYCLES))
    {
        s_busy = 1;
        matrix_push(key);
    }
}

/**
 * @brief  1ms 钩子：读上一行、驱动下一行，每 4ms 判决一次
 */
static void matrix_irq_tick(void)
{
    uint8_t cols;
    uint8_t c;
    uint8_t key = 0;

    /* 1. 读"上一拍驱动的那一行"的列线（已经稳定 1ms） */
    cols = (uint8_t)((MKEY_PORT->IDR & MKEY_COL_PINS) >> MKEY_COL_SHIFT);

    if (cols != 0x0F) /* 有列为低：这一行有键按下 */
    {
        for (c = 0; c < 4; c++)
        {
            if ((cols & (uint8_t)(1u << c)) == 0)
            {
                key = (uint8_t)((s_row << 2) + c + 1); /* 行优先：行*4 + 列 + 1 */
                break;
            }
        }

        if (key != 0)
        {
            s_cycle_key = key;
        }
    }

    /* 2. 驱动下一行：先全部拉高，再只拉低目标行 */
    MKEY_PORT->BSRR = MKEY_ROW_PINS;
    s_row = (uint8_t)((s_row + 1) & 0x03);
    MKEY_PORT->BRR = (uint16_t)(1u << (MKEY_ROW_SHIFT + s_row));

    /* 3. 刚好读完第 4 行（行号回到 0）：本轮结束，判决一次 */
    if (s_row == 0)
    {
        matrix_sample(s_cycle_key);
        s_cycle_key = 0;
    }
}

/**
 * @brief  初始化矩阵键盘
 * @note   先调 TICK_Init()；本函数会挂 1ms 钩子，最后在主程序里 TICK_Start()
 */
void MatrixIRQ_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(MKEY_RCC, ENABLE);

    /* 行：推挽输出 */
    gpio.GPIO_Pin = MKEY_ROW_PINS;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MKEY_PORT, &gpio);

    /* 列：上拉输入，按下被行线拉低 */
    gpio.GPIO_Pin = MKEY_COL_PINS;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(MKEY_PORT, &gpio);

    /* 行全高（不选中），然后先选中第 0 行 */
    MKEY_PORT->BSRR = MKEY_ROW_PINS;
    s_row = 0;
    MKEY_PORT->BRR = (uint16_t)(1u << (MKEY_ROW_SHIFT + s_row));

    s_cycle_key = 0;
    s_stable_key = 0;
    s_stable_count = 0;
    s_busy = 0;
    s_head = 0;
    s_tail = 0;

    TICK_AddHook(matrix_irq_tick);
}

/**
 * @brief  从队列取一个键值
 * @retval 1~16，没有按键返回 0
 */
uint8_t MatrixIRQ_Scan(void)
{
    uint8_t key;

    if (s_tail == s_head)
    {
        return 0;
    }

    key = s_queue[s_tail];
    s_tail = (uint8_t)((s_tail + 1) & (MKEY_QUEUE_SIZE - 1));
    return key;
}
