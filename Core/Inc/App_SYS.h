#ifndef __APP_SYS_H__
#define __APP_SYS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* --- 包含依赖 --- */
#include "main.h"       // 获取 HAL 库定义
#include "App_SHT40.h"  // 获取传感器数据结构
#include "rtc.h"        // 获取 RTC 时间句柄

/* --- 系统控制结构体 --- */
typedef struct {
    // === 输入信号 (硬件开关状态) ===
    // 逻辑值：1 = 开启/使能 (物理引脚接地), 0 = 关闭/断开 (物理引脚悬空)
    volatile uint8_t sw_master; // PB0 总控
    volatile uint8_t sw_led;    // PA7 LED
    volatile uint8_t sw_pump;   // PA6 水泵
    volatile uint8_t sw_fan;    // PA5 风扇
    
    // === 输出状态 (逻辑计算结果) ===
    // 逻辑值：1 = 工作, 0 = 停止
    uint8_t out_led;
    uint8_t out_pump;
    uint8_t out_fan;

    // === 运行控制参数 ===
    uint32_t current_interval;  // 当前采样间隔 (ms)
    uint32_t last_sample_tick;  // 上次采样的时间戳
} SYS_Ctrl_t;

/* --- 全局变量声明 --- */
extern SYS_Ctrl_t sys_ctrl;

/* --- 函数接口声明 --- */

/**
 * @brief 系统初始化
 * @note  初始化GPIO状态，设置默认采样率
 */
void App_SYS_Init(void);

/**
 * @brief 系统主业务逻辑循环
 * @param sht SHT40传感器对象指针
 * @note  需在 main 的 while(1) 中周期调用
 */
void App_SYS_Loop(SHT40_t *sht);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SYS_H__ */