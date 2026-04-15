#ifndef __APP_BLINK_H__
#define __APP_BLINK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h" // 必须包含，用于获取 HAL 库定义和 GPIO 引脚定义

/* --- 配置参数 --- */
#define LED_NORMAL_ON_MS   1000  // 正常模式：亮灯时长
#define LED_NORMAL_OFF_MS  3000  // 正常模式：灭灯时长
/* --- 报错模式参数 --- */
#define LED_FAST_MS        200   // 报错模式：亮/灭时长均为此值

/* --- 接口声明 --- */

/**
 * @brief  非阻塞 LED 闪烁任务
 * @note   请将此函数放入 main.c 的 while(1) 循环中调用
 * 它利用 HAL_GetTick() 进行时间片轮询，不会阻塞其他任务
 */
void App_Blink_Process(void);

/**
 * @brief  设置闪烁模式
 * @param  enable: 
 * 1 = 报错快闪模式 (200ms)
 * 0 = 正常慢闪模式 (1000ms/3000ms)
 */
void App_Blink_SetFastMode(uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BLINK_H__ */