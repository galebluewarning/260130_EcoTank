/*
将串口接收到的散碎字节流封装为完整的字符串指令（行数据），供上层应用调度使用。
①异步字节转字符串（拼包）：利用串口接收中断（HAL_UART_Receive_IT），在后台逐个字节采集数据。通过识别 \r 或 \n 结束符，将散乱的字节流自动拼接为标准的 C 语言字符串 rx_line。
②通信状态监控与通知：维护全局标志位 cmd_ready。当接收到完整的一行指令时将其置 1，通过“信号量”机制通知调度层（main.c）进行逻辑解析。
③内存安全与缓冲区管理：内置 RX_BUFFER_SIZE 边界检查，防止因长指令输入导致的缓冲区溢出（Stack Overflow）风险。提供 App_UART_RX_Reset 接口，用于在指令处理完成后统一清空缓存，确保下一条指令的独立性与准确性。
*/
#ifndef __APP_UART_RX_HANDLER_H__
#define __APP_UART_RX_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* --- 全局常量定义 --- */
#define RX_BUFFER_SIZE 64  // 定义接收缓冲区大小

/* --- 外部变量声明 (使用 extern 允许 main.c 访问) --- */
extern char rx_line[RX_BUFFER_SIZE];    // 拼包完成的字符串缓冲区
extern volatile uint8_t cmd_ready;       // 命令就绪标志位

/* --- 功能函数声明 --- */
/**
 * @brief 初始化串口接收管理器
 * 启动中断接收循环
 * 在int main(void){}中初始化
 */
void App_UART_RX_Init(void);

/**
 * @brief 重置接收缓冲区与标志位
 * 交还缓冲区控制权给调度层
 * 在调度层中重置接收模块
 */
void App_UART_RX_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UART_RX_HANDLER_H__ */