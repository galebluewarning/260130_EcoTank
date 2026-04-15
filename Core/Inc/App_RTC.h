#ifndef __APP_RTC_H__
#define __APP_RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**初始化 RTC 控制台交互逻辑
 * 开启串口中断。注意：内部 printf 输出依赖 App_USART_Redirect 模块。
 */
void App_RTC_Init(void);

/* * RTC 任务处理函数
 * 放入 main 函数的 while(1) 循环中调用
 * 负责解析串口命令并执行
 */
void App_RTC_ParseCommand(char *cmd_line);

/*
 * @brief 检查并同步日期到 BKP
 */
void App_RTC_CheckAndSyncBKP(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_RTC_H__ */