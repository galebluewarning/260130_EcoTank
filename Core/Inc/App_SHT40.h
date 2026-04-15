/**
 * @file    App_SHT40.h
 * @author  Gemini (Cortex-M3 Optimized)
 * @brief   SHT40 温湿度传感器应用驱动 (软件模拟 I2C 版)
 * @details 本模块实现了 SHT40 的数据采集、CRC 校验及基于 BKP 的阈值保存功能。
 */

#ifndef __APP_SHT40_H__
#define __APP_SHT40_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h" 
#include "App_Simu_I2C.h" // 必须包含底层模拟 I2C 驱动，以识别 Start/Stop/WaitAck 等函数

/* ========================== SHT40 硬件地址定义 ========================== */
/**
 * @brief SHT40 7位从机地址 (标准为 0x44)
 */
#ifndef SHT40_ADDR_7BIT
#define SHT40_ADDR_7BIT    0x44
#endif

/**
 * @brief S2C 地址字节: 7位地址左移一位 + 读写位
 */
#ifndef SHT40_ADDR_W
#define SHT40_ADDR_W       (SHT40_ADDR_7BIT << 1)       // 写地址: 0x88
#endif

#ifndef SHT40_ADDR_R
#define SHT40_ADDR_R       ((SHT40_ADDR_7BIT << 1) | 0x01) // 读地址: 0x89
#endif

/* ========================== SHT40 控制指令定义 ========================== */
#define SHT40_CMD_HIGH_PRECISION 0xFD  // 高精度测量指令 (约需 10ms 等待时间)
#define SHT40_CMD_SOFT_RESET     0x94  // 传感器软件复位指令

/**
 * @brief SHT40 内置加热器指令 (200mW, 加热 1 秒)
 */
#define SHT40_CMD_HEATER_200MW_1S_MSB  0x39
#define SHT40_CMD_HEATER_200MW_1S_LSB  0xC6

/* ========================== 数据结构定义 ========================== */
/**
 * @brief SHT40 实例管理结构体
 * 整合了实时采集数据与掉电保存的温湿度控制阈值
 */
typedef struct {
    float temperature;            // 当前温度值 (℃)
    float humidity;               // 当前相对湿度值 (%RH)
    
    /* 环境控制闭环阈值 */
    float temp_bot;               // 温度下限
    float temp_top;               // 温度上限
    float humi_bot;               // 湿度下限 (用于控制加湿水泵)
    float humi_top;               // 湿度上限 (用于控制除湿风扇)
} SHT40_t;

/* ========================== 函数原型声明 ========================== */

/**
 * @brief 初始化 SHT40 对象，加载 BKP 寄存器中的阈值
 * @param dev 指向 SHT40 实例的指针
 */
void App_SHT40_Init(SHT40_t *dev);

/**
 * @brief 触发传感器软件复位逻辑
 */
void App_SHT40_SoftReset(SHT40_t *dev);

/**
 * @brief 读取一次温湿度数据 (包含 I2C 时序、等待、CRC 校验及物理量转换)
 * @return HAL_StatusTypeDef 返回 HAL_OK 表示读取且校验成功
 */
HAL_StatusTypeDef App_SHT40_ReadTempHum(SHT40_t *dev);

/**
 * @brief 开启加热器去凝露 (阻塞 1.1s)
 */
HAL_StatusTypeDef App_SHT40_ActivateHeater(SHT40_t *dev);

/**
 * @brief 串口命令行解析，支持动态设置并保存阈值
 */
void App_SHT40_ParseCommand(SHT40_t *dev, char *cmd_line);

/**
 * @brief 立即打印一次当前数据至串口
 */
void App_SHT40_Print(SHT40_t *dev); 

/**
 * @brief 非阻塞周期性读取打印函数，适用于 main loop
 */
void App_SHT40_NEWS(SHT40_t *dev, uint32_t interval_ms);

#ifdef __cplusplus
}
#endif
#endif /* __APP_SHT40_H__ */