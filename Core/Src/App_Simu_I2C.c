#include "App_Simu_I2C.h"

/**
 * @brief  微秒级软件延时函数
 * @details 基于头文件计算出的 SIMU_I2C_DELAY_COUNT 进行原子延时。
 * volatile 关键字强制 CPU 逐次执行递减操作，避免被编译器 O3 等级优化。
 */
static void Simu_I2C_Delay(void) {
    /* 使用宏定义的计数值，实现主频与速率的自动适配 */
    volatile uint32_t i = SIMU_I2C_DELAY_COUNT; 
    while(i--);
}

/**
 * @brief 产生起始条件
 * @details SDA 线在 SCL 高电平期间由高变低，标识一次传输的开始。
 */
void App_Simu_I2C_Start(void) {
    I2C_SDA_H;           // 确保数据线释放
    I2C_SCL_H;           // 确保时钟线释放
    Simu_I2C_Delay();    // 建立时间
    I2C_SDA_L;           // SDA 下降沿，产生 START
    Simu_I2C_Delay();    // 保持时间
    I2C_SCL_L;           // 钳住时钟线，准备发送数据
}

/**
 * @brief 产生停止条件
 * @details SDA 线在 SCL 高电平期间由低变高，标识传输结束。
 */
void App_Simu_I2C_Stop(void) {
    I2C_SDA_L;           // 确保数据线为低
    I2C_SCL_L;           // 确保时钟线为低
    Simu_I2C_Delay();
    I2C_SCL_H;           // 先拉高时钟线
    Simu_I2C_Delay();    // 建立时间
    I2C_SDA_H;           // SDA 上升沿，产生 STOP
    Simu_I2C_Delay();
}

/**
 * @brief 等待从机的应答信号 (ACK)
 * @details 主机释放 SDA，脉冲 SCL，并采样 SDA 状态。
 * @return 0: 成功 (从机将 SDA 拉低); 1: 失败 (从机保持 SDA 为高)
 */
uint8_t App_Simu_I2C_WaitAck(void) {
    uint8_t timeout = 0;
    I2C_SDA_H;           // 主机释放 SDA 数据线
    Simu_I2C_Delay();
    I2C_SCL_H;           // 拉高时钟线进行采样
    Simu_I2C_Delay();
    
    /* 循环检测从机是否拉低了 SDA 线 */
    while(I2C_SDA_READ) {
        timeout++;
        if(timeout > 250) { // 超过等待上限
            App_Simu_I2C_Stop();
            return 1;       // 返回超时/非应答
        }
    }
    I2C_SCL_L;           // 采样结束，拉低时钟线准备下一位
    return 0;            // 成功接收到 ACK
}

/**
 * @brief 向总线发送一个字节
 * @param data 按 MSB (最高有效位) 顺序依次发送数据位
 */
void App_Simu_I2C_SendByte(uint8_t data) {
    for(uint8_t i = 0; i < 8; i++) {
        /* 判断当前最高位是 1 还是 0 */
        if(data & 0x80) I2C_SDA_H; else I2C_SDA_L;
        Simu_I2C_Delay();
        
        I2C_SCL_H;       // 抬高时钟线，告知从机此时数据有效
        Simu_I2C_Delay();
        I2C_SCL_L;       // 拉低时钟线，允许改变 SDA 状态
        
        data <<= 1;      // 左移一位，处理下一 bit
    }
}

/**
 * @brief 从总线读取一个字节并反馈应答
 * @param ack 1 表示发送 ACK 给从机，0 表示发送 NACK 给从机 (通常用于读取最后一字节)
 * @return uint8_t 读取到的数据字节
 */
uint8_t App_Simu_I2C_ReadByte(uint8_t ack) {
    uint8_t receive = 0;
    I2C_SDA_H;           // 主机释放数据线进入输入/接收状态
    
    for(uint8_t i = 0; i < 8; i++) {
        I2C_SCL_L;       // 准备读取
        Simu_I2C_Delay();
        I2C_SCL_H;       // 拉高时钟线，数据在此期间应保持稳定
        receive <<= 1;   // 移位准备存储
        if(I2C_SDA_READ) receive++; // 如果物理电平为高，则将该位置 1
        Simu_I2C_Delay();
    }
    
    /* --- 产生应答或非应答脉冲 --- */
    I2C_SCL_L;           // 结束数据读取
    if(ack) I2C_SDA_L; else I2C_SDA_H; // 根据要求拉低 (ACK) 或拉高 (NACK) SDA
    Simu_I2C_Delay();
    I2C_SCL_H;           // 发送第 9 个时钟脉冲
    Simu_I2C_Delay();
    I2C_SCL_L;           // 释放总线
    
    return receive;
}