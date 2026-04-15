/**
 * @file    App_Simu_I2C.h
 * @brief   软件模拟 I2C 驱动模块头文件
 * @details 本模块通过 GPIO 模拟 I2C 协议的时序，专门适配开漏输出 (Open-Drain) 模式。
 */

#ifndef __APP_SIMU_I2C_H
#define __APP_SIMU_I2C_H

#include "main.h"


/* ========================== 核心频率与速率配置 ========================== */
/**
 * @brief  系统主频配置 (单位: MHz)
 * @note   对应 SystemCoreClock / 1000000。例如 STM32F103 标称 72MHz。
 */
#define SIMU_I2C_SYS_FREQ_MHZ    72

/**
 * @brief  I2C 通信目标速率 (单位: kHz)
 * @note   标准模式(Standard)通常为 100kHz，快速模式(Fast)可设为 400kHz。
 * SHT40 建议使用 100kHz-400kHz 以保证信号完整性。
 */
#define SIMU_I2C_BITRATE_KHZ     100

/**
 * @brief  自动换算延时计数值
 * @details 计算逻辑：(系统频率 * 1000) / (目标速率 * 指令周期因子)
 * 指令周期因子 18 是针对 Cortex-M3 (72MHz) 优化所得的经验常数。
 */
#define SIMU_I2C_DELAY_COUNT     ((SIMU_I2C_SYS_FREQ_MHZ * 1000) / (SIMU_I2C_BITRATE_KHZ * 18))


/* ========================== 硬件引脚配置宏 ========================== */
/** * @note 必须在 CubeMX 中将 PB6/PB7 配置为 Output Open Drain 模式，且 Speed 为 High。
 * 外部需接入 2.2kΩ - 4.7kΩ 的上拉电阻。
 */
#define SIMU_I2C_PORT      GPIOB
#define SIMU_I2C_SCL_PIN   GPIO_PIN_6
#define SIMU_I2C_SDA_PIN   GPIO_PIN_7

/* ========================== 底层电平控制宏 ========================== */
/** * @brief SCL 线拉高：释放总线，由外部电阻拉至高电平 
 */
#define I2C_SCL_H    HAL_GPIO_WritePin(SIMU_I2C_PORT, SIMU_I2C_SCL_PIN, GPIO_PIN_SET)

/** * @brief SCL 线拉低：强行将时钟线拉至地电平 
 */
#define I2C_SCL_L    HAL_GPIO_WritePin(SIMU_I2C_PORT, SIMU_I2C_SCL_PIN, GPIO_PIN_RESET)

/** * @brief SDA 线拉高：释放数据线，允许从机控制或作为高电平输出 
 */
#define I2C_SDA_H    HAL_GPIO_WritePin(SIMU_I2C_PORT, SIMU_I2C_SDA_PIN, GPIO_PIN_SET)

/** * @brief SDA 线拉低：强行将数据线拉至地电平 
 */
#define I2C_SDA_L    HAL_GPIO_WritePin(SIMU_I2C_PORT, SIMU_I2C_SDA_PIN, GPIO_PIN_RESET)

/** * @brief 读取 SDA 线状态：直接读取 IDR 寄存器，获取当前总线物理电平 
 */
#define I2C_SDA_READ HAL_GPIO_ReadPin(SIMU_I2C_PORT, SIMU_I2C_SDA_PIN)

/* ========================== I2C 协议层函数声明 ========================== */

/**
 * @brief 产生 I2C 起始信号
 * @details 在 SCL 为高电平时，SDA 产生一个由高到低的跳变。
 */
void App_Simu_I2C_Start(void);

/**
 * @brief 产生 I2C 停止信号
 * @details 在 SCL 为高电平时，SDA 产生一个由低到高的跳变。
 */
void App_Simu_I2C_Stop(void);

/**
 * @brief 发送一个字节数据
 * @param data 待发送的 8 位数据 (MSB 先行)
 */
void App_Simu_I2C_SendByte(uint8_t data);

/**
 * @brief 读取一个字节数据并返回应答信号
 * @param ack 1: 发送应答 (ACK); 0: 发送非应答 (NACK)
 * @return uint8_t 读取到的 8 位数据
 */
uint8_t App_Simu_I2C_ReadByte(uint8_t ack);

/**
 * @brief 等待从机应答信号 (ACK)
 * @return uint8_t 0: 接收到 ACK (成功); 1: 未接收到应答 (失败/超时)
 */
uint8_t App_Simu_I2C_WaitAck(void);

#endif /* __APP_SIMU_I2C_H */