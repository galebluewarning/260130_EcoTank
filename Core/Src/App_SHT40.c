/**
 * @file    App_SHT40.c
 * @brief   SHT40 驱动实现，包含 CRC8 算法与 BKP 阈值管理逻辑
 */

#include "App_SHT40.h"
#include "rtc.h"       // 用于操作备份寄存器 (BKP)
#include <stdio.h>
#include <string.h>

/* ========================== 备份寄存器分配 (BKP) ========================== */
/* STM32F103 的 DR5-DR8 映射至温湿度阈值，DR9 存储初始化魔术数 */
/* DR1 - DR4 已被 RTC 占用 */
/* SHT40 占用 DR5 - DR8 */
#define BKP_DR_TEMP_BOT  RTC_BKP_DR5
#define BKP_DR_TEMP_TOP  RTC_BKP_DR6
#define BKP_DR_HUMI_BOT  RTC_BKP_DR7
#define BKP_DR_HUMI_TOP  RTC_BKP_DR8
#define SHT40_BKP_MAGIC    0xA5A5       // 用于验证 BKP 数据是否有效的标识
#define BKP_DR_SHT40_CHK   RTC_BKP_DR9  

/********************************* 内部辅助函数 ***********************************/

/**
 * @brief SHT40 官方 CRC8 校验算法
 * @details 多项式: 0x31 (x^8 + x^5 + x^4 + 1), 初始值: 0xFF
 */
static uint8_t SHT40_CalcCRC(uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief 将阈值保存至备份寄存器
 * @details 算法: float * 100 转换为整数存储，以保留两位小数精度
 */
static void SaveThresholdToBKP(uint32_t BackupRegister, float value) {
    HAL_PWR_EnableBkUpAccess(); // 开启备份域访问权限
    HAL_RTCEx_BKUPWrite(&hrtc, BackupRegister, (uint32_t)((int16_t)(value * 100.0f)));
}

/********************************* 核心功能实现 ***********************************/

/**
 * @brief 软件复位传感器
 * @details 通过模拟 I2C 发送 0x94 复位指令
 */
void App_SHT40_SoftReset(SHT40_t *dev) {
    App_Simu_I2C_Start();
    App_Simu_I2C_SendByte(SHT40_ADDR_W);
    if(App_Simu_I2C_WaitAck() == 0) {
        App_Simu_I2C_SendByte(SHT40_CMD_SOFT_RESET);
        App_Simu_I2C_WaitAck();
    }
    App_Simu_I2C_Stop();
    HAL_Delay(10); // 等待传感器完成内部复位
}

/**
 * @brief 初始化并加载掉电参数
 */
void App_SHT40_Init(SHT40_t *dev) {

    /* 确保电源和备份域时钟开启 */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* 检查 BKP 验证位是否匹配魔术数 */
    if (HAL_RTCEx_BKUPRead(&hrtc, BKP_DR_SHT40_CHK) == SHT40_BKP_MAGIC) {
    /* 尝试从 BKP 读取历史阈值 (宏函数：读出 uint32 -> 转 int16 -> 除 100.0) */
        #define READ_BKP_VAL(reg)  ( (int16_t)HAL_RTCEx_BKUPRead(&hrtc, reg) / 100.0f )
        dev->temp_bot = READ_BKP_VAL(BKP_DR_TEMP_BOT);
        dev->temp_top = READ_BKP_VAL(BKP_DR_TEMP_TOP);
        dev->humi_bot = READ_BKP_VAL(BKP_DR_HUMI_BOT);
        dev->humi_top = READ_BKP_VAL(BKP_DR_HUMI_TOP);
    } 
    else {
        /* 首次运行或电池没电，加载出厂默认参数 */
        dev->temp_bot = 20.0f; dev->temp_top = 30.0f;
        dev->humi_bot = 70.0f; dev->humi_top = 85.0f;
        
        SaveThresholdToBKP(BKP_DR_TEMP_BOT, dev->temp_bot);
        SaveThresholdToBKP(BKP_DR_TEMP_TOP, dev->temp_top);
        SaveThresholdToBKP(BKP_DR_HUMI_BOT, dev->humi_bot);
        SaveThresholdToBKP(BKP_DR_HUMI_TOP, dev->humi_top);
        
        HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR_SHT40_CHK, SHT40_BKP_MAGIC);
    }

    App_SHT40_SoftReset(dev); 

    /* 参数清洗
     * ===========================================================
     * 安全阈值检查 (防止 Bot >= Top 导致设备同时开启)
     * 强制约束：Bot 必须至少比 Top 小 10% (留出缓冲区)
     * ============================================================ */
    if (dev->temp_bot >= dev->temp_top) {
        dev->temp_bot = dev->temp_top - 5.0f; // 自动修正温度
    }
    // 关键：湿度保护
    // 假设 Top=80, 则 Bot 不能超过 70。防止 Top(80) < Hum(75) < Bot(90) 的情况
    if (dev->humi_bot >= (dev->humi_top - 5.0f)) {
        dev->humi_bot = dev->humi_top - 10.0f; // 强制拉开 10% 的差距
    }

    printf("\r\n[App_SHT40 Init]\r\n");
    printf("[Humi%.1f%%-%.1f%%]\r\n", 
           dev->humi_bot, dev->humi_top);
    printf("[1.set humi_bot|2.set humi_top]\r\n\r\n");
        App_SHT40_Print(dev); 
}

/**
 * @brief 核心数据采集函数 (模拟 I2C 流程)
 */
HAL_StatusTypeDef App_SHT40_ReadTempHum(SHT40_t *dev) {
    uint8_t buffer[6]; // [T_MSB, T_LSB, T_CRC, H_MSB, H_LSB, H_CRC]

    /* 1. 发送触发转换指令 */
    App_Simu_I2C_Start();
    App_Simu_I2C_SendByte(SHT40_ADDR_W);
    if(App_Simu_I2C_WaitAck()) { App_Simu_I2C_Stop(); return HAL_ERROR; }
    App_Simu_I2C_SendByte(SHT40_CMD_HIGH_PRECISION);
    if(App_Simu_I2C_WaitAck()) { App_Simu_I2C_Stop(); return HAL_ERROR; }
    App_Simu_I2C_Stop();

    /* 2. 等待高精度测量完成 (典型值 8.3ms) */
    HAL_Delay(10); 

    /* 3. 顺序读取 6 个字节原始数据 */
    App_Simu_I2C_Start();
    App_Simu_I2C_SendByte(SHT40_ADDR_R);
    if(App_Simu_I2C_WaitAck()) { App_Simu_I2C_Stop(); return HAL_ERROR; }
    
    for(int i = 0; i < 5; i++) buffer[i] = App_Simu_I2C_ReadByte(1); // 发送 ACK 继续读
    buffer[5] = App_Simu_I2C_ReadByte(0);                           // 最后一字节发 NACK
    App_Simu_I2C_Stop();

    /* 4. 数据校验: 只有通过 CRC 校验的数据才会被更新至 dev */
    if (SHT40_CalcCRC(buffer, 2) != buffer[2]) return HAL_ERROR;
    if (SHT40_CalcCRC(&buffer[3], 2) != buffer[5]) return HAL_ERROR;

    /* 5. 物理量换算公式 (源自手册) */
    uint16_t t_ticks = (buffer[0] << 8) | buffer[1];
    uint16_t h_ticks = (buffer[3] << 8) | buffer[4];
    dev->temperature = -45.0f + 175.0f * ((float)t_ticks / 65535.0f);
    dev->humidity = -6.0f + 125.0f * ((float)h_ticks / 65535.0f);

    // 6.湿度限幅 (0-100%)
    if (dev->humidity > 100.0f) dev->humidity = 100.0f;
    if (dev->humidity < 0.0f) dev->humidity = 0.0f;

    return HAL_OK;
}

/**
 * @brief 激活 SHT40 内置加热器
 */
HAL_StatusTypeDef App_SHT40_ActivateHeater(SHT40_t *dev) {
    App_Simu_I2C_Start();
    App_Simu_I2C_SendByte(SHT40_ADDR_W);
    if(App_Simu_I2C_WaitAck()) { App_Simu_I2C_Stop(); return HAL_ERROR; }
    App_Simu_I2C_SendByte(SHT40_CMD_HEATER_200MW_1S_MSB);
    App_Simu_I2C_WaitAck();
    App_Simu_I2C_SendByte(SHT40_CMD_HEATER_200MW_1S_LSB);
    App_Simu_I2C_WaitAck();
    App_Simu_I2C_Stop();
    
    HAL_Delay(1100); // 阻塞等待 1 秒加热周期完成
    return HAL_OK;
}

/**
 * @brief 解析串口命令修改阈值，并写入 BKP
 * 命令格式: set temp_top 28.5
 */
void App_SHT40_ParseCommand(SHT40_t *dev, char *cmd_line) {
    float val;
    
    if (sscanf(cmd_line, "set temp_bot %f", &val) == 1) {
        /* 检查：下限必须与当前上限保持至少 5.0℃ 的安全裕度 */
        if (val > (dev->temp_top - 5.0f)) {
            printf("[App_SHT40 temp_bot must be <= temp_top - 5.0 (Curr: %.2f)]\r\n", dev->temp_top);
            return;
        }
        dev->temp_bot = val;
        SaveThresholdToBKP(BKP_DR_TEMP_BOT, val); // 保存到掉电区
        printf("[App_SHT40 temp_bot set to %.2f℃]\r\n", val);
    } 
    else if (sscanf(cmd_line, "set temp_top %f", &val) == 1) {
        /* 检查：上限必须与当前下限保持至少 5.0℃ 的安全裕度 */
        if (val < (dev->temp_bot + 5.0f)) {
            printf("[App_SHT40 temp_top must be >= temp_bot + 5.0 (Curr: %.2f)]\r\n", dev->temp_bot);
            return;
        }
        dev->temp_top = val;
        SaveThresholdToBKP(BKP_DR_TEMP_TOP, val); // 保存到掉电区
        printf("[App_SHT40 temp_top set to %.2f℃]\r\n", val);
    }
    else if (sscanf(cmd_line, "set humi_bot %f", &val) == 1) {
        /* 检查：湿度下限必须与当前上限保持至少 10.0% 的安全裕度 */
        if (val > (dev->humi_top - 10.0f)) {
            printf("[App_SHT40 humi_bot must be <= humi_top - 10.0 (Curr: %.2f)]\r\n", dev->humi_top);
            return;
        }
        dev->humi_bot = val;
        SaveThresholdToBKP(BKP_DR_HUMI_BOT, val); // 保存到掉电区
        printf("[App_SHT40 humi_bot set to %.2f%%]\r\n", val);
    }
    else if (sscanf(cmd_line, "set humi_top %f", &val) == 1) {
        /* 检查：湿度上限必须与当前下限保持至少 10.0% 的安全裕度 */
        if (val < (dev->humi_bot + 10.0f)) {
            printf("[App_SHT40 humi_top must be >= humi_bot + 10.0 (Curr: %.2f)]\r\n", dev->humi_bot);
            return;
        }
        dev->humi_top = val;
        SaveThresholdToBKP(BKP_DR_HUMI_TOP, val); // 保存到掉电区
        printf("[App_SHT40 humi_top set to %.2f%%]\r\n", val);
    }
}


/**
 * @brief 读取并打印结果 (一次性操作)
 */
void App_SHT40_Print(SHT40_t *dev) {
    if (App_SHT40_ReadTempHum(dev) == HAL_OK) 
        printf("Temp:%.2f℃ Humi:%.2f%%\r\n", dev->temperature, dev->humidity);
    else 
        printf("[App_SHT40 Read Error]\r\n");
}

/**
 * @brief 基于 HAL_GetTick() 的非阻塞周期采集
 */
void App_SHT40_NEWS(SHT40_t *dev, uint32_t interval_ms) {
    static uint32_t news_tick = 0;
    if (HAL_GetTick() - news_tick >= interval_ms) { 
        news_tick = HAL_GetTick(); 
        App_SHT40_Print(dev); 
    }
}