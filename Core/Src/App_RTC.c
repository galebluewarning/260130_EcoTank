#include "App_RTC.h"
#include "rtc.h"
#include <stdio.h>
#include <string.h>

/* --- 私有宏定义 --- */
#define RTC_BKP_MAGIC     0x32F2  // 用于标记"时间已设置"的魔术数
/* RTC 占用 DR1 - DR4 */
#define RTC_BKP_DR_TIME   RTC_BKP_DR1   // DR1: 存放初始化标记 (Magic Number)
#define RTC_BKP_DR_YEAR   RTC_BKP_DR2   // DR2: 存年份 (0-99)
#define RTC_BKP_DR_MONTH  RTC_BKP_DR3   // DR3: 存月份 (1-12)
#define RTC_BKP_DR_DATE   RTC_BKP_DR4   // DR4: 存日期 (1-31)


/*************************************************内部辅助函数*****************************************************/

/**
 * @brief 将日期保存到备份寄存器 (DR2-DR4)
 * 防止 STM32F1 掉电/复位后日期丢失
 */
static void SaveDateToBKP(uint8_t year, uint8_t month, uint8_t date) {
    /* 必须先开启 BKP 访问权限 */
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR_YEAR, year);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR_MONTH, month);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR_DATE, date);
}

/*************************************************功能函数实现*****************************************************/


/**
 * @brief 应用层 RTC 逻辑入口
 * @note  必须在 MX_RTC_Init() 之后调用
 * 在rtc.c USER CODE Check_RTC_BKUP间加入 return;
 * 保留rtc.c硬件初始化，但在设置默认时间之前强行退出。
 */
void App_RTC_Init(void) {

    // 1. 开启电源和备份接口时钟 (访问 BKP 必须)
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();


    // 2. 检查 BKP 标记：判断是冷启动还是热启动
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR_TIME) != RTC_BKP_MAGIC) {
        
        /* === 情况 A: 冷启动 (电池没电 或 第一次刷程序) === */
        printf("\r\n[App_RTC first init]\r\n");

        // 设置默认时间: 12:00:00
        RTC_TimeTypeDef sTime = {12, 0, 0};
        // 设置默认日期: 2026-03-02
        RTC_DateTypeDef sDate = {RTC_WEEKDAY_MONDAY, RTC_MONTH_MARCH, 2, 26};

        // 写入硬件
        if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
             printf("\r\n[App_RTC first init failed]\r\n");
        }
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        
        // [关键] 手动把默认日期写入 BKP，否则下次热启动读出来是 0
        SaveDateToBKP(26, 3, 2); // 2026-03-02
        // 写入魔术数，告诉下次启动：“时间已设置，请勿重置”
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR_TIME, RTC_BKP_MAGIC);

    } else {
        
        /* === 情况 B: 热启动 (有电池，RTC 正在运行) === */
        printf("\r\n[App_RTC Warm Boot]\r\n");
        
        // 等待同步，确保能读取寄存器
        HAL_RTC_WaitForSynchro(&hrtc);
    
        /* 从 BKP 恢复日期到 RAM */
        // 因为 F1 的硬件不存日期，重启后 HAL 库里的日期变量会丢失，必须从 BKP 填回去
        uint8_t bkp_year  = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR_YEAR);
        uint8_t bkp_month = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR_MONTH);
        uint8_t bkp_date  = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR_DATE);

        // 简单的合法性检查 (防止 BKP 数据损坏导致日期为0)
        if (bkp_month == 0) bkp_month = 1; 
        if (bkp_date == 0)  bkp_date = 1;

        RTC_DateTypeDef sDate = {0};
        sDate.Year  = bkp_year;
        sDate.Month = bkp_month;
        sDate.Date  = bkp_date;
        sDate.WeekDay = RTC_WEEKDAY_MONDAY; // 星期暂不计算，如有需要可加算法

        // 恢复到 HAL 库句柄中
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        
}

    // 3. 打印交互菜单
   // 注意：若未完成串口重定向，以下 printf 将失效
    printf("[1.show | 2.set time HH:MM:SS | 3.set date YY-MM-DD]\r\n");

			/* 4.初始化完成后立即显示当前 RTC 状态 */
    RTC_TimeTypeDef sTimeNow = {0};
    RTC_DateTypeDef sDateNow = {0};
    // 获取当前硬件寄存器值
    HAL_RTC_GetTime(&hrtc, &sTimeNow, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDateNow, RTC_FORMAT_BIN);
    
    printf("20%02d-%02d-%02d %02d:%02d:%02d\r\n", 
           sDateNow.Year, sDateNow.Month, sDateNow.Date, 
           sTimeNow.Hours, sTimeNow.Minutes, sTimeNow.Seconds);
}

/**
 * @brief RTC 模块专用命令解析器
 * @param cmd_line 传入待解析的字符串
 */
void App_RTC_ParseCommand(char *cmd_line)
 {
    // 1. 命令：show
    if (strncmp(cmd_line, "show", 4) == 0) {
        RTC_TimeTypeDef sTime = {0};
        RTC_DateTypeDef sDate = {0};
        HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        printf("20%02d-%02d-%02d %02d:%02d:%02d\r\n", 
               sDate.Year, sDate.Month, sDate.Date, sTime.Hours, sTime.Minutes, sTime.Seconds);
    }
    // 2. 命令：set time
    else if (strncmp(cmd_line, "set time", 8) == 0) {
        int h, m, s;
        if (sscanf(cmd_line, "set time %d:%d:%d", &h, &m, &s) == 3) {
            RTC_TimeTypeDef sTime = {(uint8_t)h, (uint8_t)m, (uint8_t)s};
            if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK)
                printf("[App_RTC Time Set]\r\n");
                // 设置时间后，刷新一下魔术数
                HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR_TIME, RTC_BKP_MAGIC);
        }
    }
    // 3. 命令：set date
    else if (strncmp(cmd_line, "set date", 8) == 0) {
        int y, m, d;
        if (sscanf(cmd_line, "set date %d-%d-%d", &y, &m, &d) == 3) {
            // [优化] 兼容 2026 和 26 两种输入格式
            if (y >= 2000) y -= 2000;
            RTC_DateTypeDef sDate = {RTC_WEEKDAY_MONDAY, (uint8_t)m, (uint8_t)d, (uint8_t)y};
            if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) == HAL_OK){
                // [关键] 成功设置日期后，立即同步保存到 BKP (DR2-DR4)
                SaveDateToBKP((uint8_t)y, (uint8_t)m, (uint8_t)d);
                printf("[App_RTC Date Set]\r\n");
                HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR_TIME, RTC_BKP_MAGIC);
            }
        }
    }
}


/**
 * @brief 检查并同步日期到 BKP
 * @note  建议在 main while(1) 或 App_SYS_Loop 中每分钟调用一次
 */
void App_RTC_CheckAndSyncBKP(void) {
    RTC_DateTypeDef sDate;
    RTC_TimeTypeDef sTime;
    
    // 1. 获取当前系统时间/日期 (必须先读 Time 再读 Date 以解锁影子寄存器)
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    // 2. 读取 BKP 中的旧日期
    uint8_t bkp_date = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR_DATE);

    // 3. 对比：如果当前日期(sDate.Date) 和 BKP里的不一致
    if (sDate.Date != bkp_date) {
        printf("\r\n[App_RTC Updating BKP:%02d->%02d]\r\n", 
               bkp_date, sDate.Date);
               
        // 4. 将新日期写入 BKP
        SaveDateToBKP(sDate.Year, sDate.Month, sDate.Date);
        
        // 5. 再次写入魔术数，确保稳固
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR_TIME, RTC_BKP_MAGIC);
    }
}

