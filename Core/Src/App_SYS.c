#include "App_SYS.h"
#include <stdio.h>
#include "App_Blink.h"

/* 全局控制实例 */
SYS_Ctrl_t sys_ctrl;

/* 引用外部 RTC 句柄 (由 CubeMX 生成在 main.c 或 rtc.c) */
extern RTC_HandleTypeDef hrtc;

/* ================= 内部静态辅助函数 ================= */

/**
 * @brief 读取物理开关电平并转换为逻辑状态
 * @note  硬件设计：开关一端接地，引脚上拉。
 * - 开关闭合 -> 引脚低电平(RESET) -> 逻辑 1 (Enable)
 * - 开关断开 -> 引脚高电平(SET)   -> 逻辑 0 (Disable)
 */
static void SYS_Update_Switch_State(void) {
    sys_ctrl.sw_master = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) ? 1 : 0;
    sys_ctrl.sw_led    = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_RESET) ? 1 : 0;
    sys_ctrl.sw_pump   = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_RESET) ? 1 : 0;
    sys_ctrl.sw_fan    = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) ? 1 : 0;
}

/**
 * @brief 获取当前 RTC 小时数 (24小时制)
 * @return hour (0-23)
 */
static uint8_t SYS_Get_RTC_Hour(void) {
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    
    // 必须同时读取 Time 和 Date 以解锁 RTC 影子寄存器，确保数据同步
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); 
    
    return sTime.Hours;
}

/**
 * @brief 打印当前 RTC 时间 (YY:MM:DD HH:MM:SS)
 */
static void SYS_Print_Time(void) {
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    
    // 必须同时读取 Time 和 Date 以解锁 RTC 影子寄存器
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); 
    
    // 2. sDate.Year 只有后两位 (如 26)，前面手动补 "20" 变成 2026
    printf("\r\n20%02d-%02d-%02d %02d:%02d:%02d\r\n", 
           sDate.Year, sDate.Month, sDate.Date, 
           sTime.Hours, sTime.Minutes, sTime.Seconds);
}

/**
 * @brief 安全停机：强制关闭所有输出设备
 */
static void SYS_Shutdown_All(void) {
    // 1. 硬件关断 (LED, 水泵, 风扇)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14, GPIO_PIN_RESET);
    
    // 2. 逻辑状态清零
    sys_ctrl.out_led = 0;
    sys_ctrl.out_pump = 0;
    sys_ctrl.out_fan = 0;
}

/* ================= 外部接口函数实现 ================= */

/**
 * @brief 初始化函数
 */
void App_SYS_Init(void) {
    // 1. 读取初始开关状态
    SYS_Update_Switch_State();
    
    // 2. 确保设备处于关闭状态
    SYS_Shutdown_All();
    
    // 3. 初始化采样参数
    sys_ctrl.current_interval = 600000; // 默认空闲模式：10分钟
    //last_sample_tick设为 0 会导致上电后等待 10 分钟。应设为"过去的时间"以立即触发。
    sys_ctrl.last_sample_tick = HAL_GetTick() - sys_ctrl.current_interval;

    printf("\r\n[App_SYS Init]\r\n");
		printf("[ALL:%d  LED:%d  PUMP:%d  FAN:%d]\r\n",sys_ctrl.sw_master, sys_ctrl.sw_led, sys_ctrl.sw_pump, sys_ctrl.sw_fan);
}

/**
 * @brief HAL库 GPIO 外部中断回调
 * @note  任何相关引脚电平变化立即更新全局状态
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if(GPIO_Pin == GPIO_PIN_0 || GPIO_Pin == GPIO_PIN_5 || 
       GPIO_Pin == GPIO_PIN_6 || GPIO_Pin == GPIO_PIN_7) {
        SYS_Update_Switch_State();
    }
}

/**
 * @brief 系统核心逻辑循环
 * @param sht 传感器对象指针
 */
void App_SYS_Loop(SHT40_t *sht) {
    uint8_t allow_running = 0;
    static uint8_t last_read_error = 0; //记录上次SHT40读取是否失败
    static uint8_t error_count = 0; //连续错误计数器
    /* ---------------------------------------------------------
     * 第一级判断：PB0 总开关
     * --------------------------------------------------------- */
    if (sys_ctrl.sw_master == 0) {
        SYS_Shutdown_All();
        return; // 总闸关闭，直接退出，不执行后续逻辑
    }


     // 1.LED 控制逻辑 (只受master和LED开关限制)
    if (sys_ctrl.sw_led) {
        sys_ctrl.out_led = 1;
    } else {
        sys_ctrl.out_led = 0;
    }
    // 立即执行一次引脚同步，确保 LED 响应最快
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, sys_ctrl.out_led ? GPIO_PIN_SET : GPIO_PIN_RESET);
    

    /* ---------------------------------------------------------
     * 第二级判断：RTC 时间窗口 (10:00:00 - 20:59:59)
     * --------------------------------------------------------- */
    uint8_t hour = SYS_Get_RTC_Hour();
    if (hour >= 10 && hour < 21) {
        allow_running = 1;
    } else {
				// 1. 独立清零水泵与风扇的逻辑状态，保障 LED 状态的延续性
        sys_ctrl.out_pump = 0;
        sys_ctrl.out_fan  = 0;
        
        // 2. 定向关断水泵(PB13)与风扇(PB14)的物理引脚
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13 | GPIO_PIN_14, GPIO_PIN_RESET);
        
        // 3. 截断主循环，屏蔽后续的环境检测与硬件响应逻辑
        return; // 时间不满足，强制关闭
    }

    /* ---------------------------------------------------------
     * 第三级判断：业务逻辑与数据采集
     * --------------------------------------------------------- */
    

     if (allow_running) {
        
        // === A. 动态采样率决策 (解决资源竞争的核心) ===
        // 只要水泵或风扇处于"工作开启"状态，或上次读取失败
        // 进入高速采样(3秒)，否则维持低速(10分钟)
        if (sys_ctrl.out_pump == 1 || sys_ctrl.out_fan == 1 || last_read_error == 1) {
            sys_ctrl.current_interval = 3000;   // 3秒
        } else {
            sys_ctrl.current_interval = 600000; // 10分钟
        }

        // === B. 执行传感器读取 ===
        // 唯一的 SHT40 读取入口，避免多处调用冲突
        if (HAL_GetTick() - sys_ctrl.last_sample_tick >= sys_ctrl.current_interval) {
            sys_ctrl.last_sample_tick = HAL_GetTick();
            
            // 调用底层驱动读取 (注意：此处不使用加热功能，防止阻塞)
            if (App_SHT40_ReadTempHum(sht) == HAL_OK) {
                SYS_Print_Time();
                printf("\r\n[App_SYS Sampled:Humi=%.1f%% (Interval: %u ms)]\r\n", 
                       sht->humidity, sys_ctrl.current_interval);
                       last_read_error = 0; // 读取成功，清除错误标记
                       error_count = 0;     // 清除连续错误计数
                       App_Blink_SetFastMode(0);//PC13正常慢闪
            } else {
                SYS_Print_Time();
                printf("\r\n[App_SYS Sensor Read Error]\r\n");
                //错误计数
                error_count++;
                App_Blink_SetFastMode(1);//PC13报错快闪

                printf("\r\n[App_SYS Emergency Stop]\r\n");
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13 | GPIO_PIN_14, GPIO_PIN_RESET);
                sys_ctrl.out_pump = 0;
                sys_ctrl.out_fan = 0;
                last_read_error = 1;   //标记错误，确保下一轮循环保持 3秒 重试间隔
                
                if (error_count >= 10) {
                    SYS_Print_Time();
                    printf("\r\n[App_SYS Sensor Failed 30s]\r\n");
                    Error_Handler();
                }

                return; // 跳过后续逻辑
            }
        }

        // === C. 设备逻辑控制 (仅读取数据，不操作硬件) ===


        // --- 2. 水泵 ---
        if (sys_ctrl.sw_pump) {
						// 开启阈值 = 下限
            if (sht->humidity < sht->humi_bot) {
                sys_ctrl.out_pump = 1; // 湿度过低，开启加湿
            } 
            // 停止阈值 = 上限 - 5%，防止在临界点反复震荡
            else if (sht->humidity > (sht->humi_top - 5.0f)) {
                sys_ctrl.out_pump = 0; // 湿度达标，停止
            }
            // 中间区间保持上一状态 (隐式迟滞)
        } else {
            sys_ctrl.out_pump = 0;
        }

        // --- 3. 风扇 ---
        if (sys_ctrl.sw_fan) {
            //开启阈值 = 上限 + 5%，防止在临界点反复震荡
            if (sht->humidity > (sht->humi_top + 5.0f)) {
                sys_ctrl.out_fan = 1; // 湿度过高，开启除湿
            }
            // 停止阈值 = 上限 - 5%，防止在临界点反复震荡
            else if (sht->humidity <= (sht->humi_top - 5.0f)) {
                sys_ctrl.out_fan = 0; 
            }
        } else {
            sys_ctrl.out_fan = 0;
        }

        // === D. 物理层输出执行 ===
        // 统一在最后一步写入硬件，逻辑清晰
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, sys_ctrl.out_pump ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, sys_ctrl.out_fan  ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
