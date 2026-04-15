#include "App_Blink.h"

/* --- 模块内部状态变量 (静态全局) --- */
/* 这些变量控制着 Process 函数中的闪烁节奏 */
static uint32_t s_blink_on_time  = LED_NORMAL_ON_MS;  // 当前亮灯时长
static uint32_t s_blink_off_time = LED_NORMAL_OFF_MS; // 当前灭灯时长


/**
 * @brief  非阻塞 LED 闪烁处理函数
 * 逻辑：PC13 低电平点亮 (Common Anode / Sink Mode)
 */
void App_Blink_Process(void) {
    /* * 使用 static 静态变量，确保函数退出后数据不丢失
     * 这些变量只初始化一次，像全局变量一样驻留内存，但只对本函数可见
     */
    static uint32_t led_tick = 0;
    static uint8_t  led_state = 1; // 初始状态标记: 1=亮, 0=灭

    /* 获取当前系统滴答 */
    uint32_t current_time = HAL_GetTick();

    if (led_state == 1) {
        /* === 当前是 [亮] 状态 === */
        /* 检查是否亮够了设定时间 */
        if (current_time - led_tick >= s_blink_on_time) {
            // 执行动作：熄灭 (PC13 Set 为高电平熄灭)
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            
            // 状态流转
            led_state = 0;           // 标记为灭
            led_tick = current_time; // 更新时间戳
        }
    } else {
        /* === 当前是 [灭] 状态 === */
        /* 检查是否灭够了设定时间 */
            if (current_time - led_tick >= s_blink_off_time) {
            // 执行动作：点亮 (PC13 Reset 为低电平点亮)
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            
            // 状态流转
            led_state = 1;           // 标记为亮
            led_tick = current_time; // 更新时间戳
        }
    }
}


/**
 * @brief  设置闪烁模式 (外部调用接口)
 */
void App_Blink_SetFastMode(uint8_t enable) {
    if (enable) {
        // === 切换为快闪模式 (报错) ===
        s_blink_on_time  = LED_FAST_MS;
        s_blink_off_time = LED_FAST_MS;
    } else {
        // === 恢复为慢闪模式 (正常) ===
        s_blink_on_time  = LED_NORMAL_ON_MS;
        s_blink_off_time = LED_NORMAL_OFF_MS;
    }
}
