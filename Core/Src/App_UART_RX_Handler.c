#include "App_UART_RX_Handler.h"
#include "usart.h"
#include <string.h>

/* --- 私有变量定义 --- */
static uint8_t rx_byte;               // 接收单个字节的中断缓存
static uint8_t rx_idx = 0;            // 当前缓冲区索引

/* --- 全局变量定义 --- */
char rx_line[RX_BUFFER_SIZE];         //
volatile uint8_t cmd_ready = 0;       //

/**
 * @brief 引用硬件句柄 (通常在 usart.c 中定义)
 */
extern UART_HandleTypeDef huart1;

void App_UART_RX_Init(void) {
    memset(rx_line, 0, RX_BUFFER_SIZE);
    rx_idx = 0;
    cmd_ready = 0;
    // 启动首次中断接收
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void App_UART_RX_Reset(void) {
    // 1. 先清空缓冲区 (防止清空标志位后新数据进来被误删)
    memset(rx_line, 0, RX_BUFFER_SIZE);
    
    // 2. [新增] 确保索引归零 (虽然 ISR 里归零了，但双重保险更稳)
    rx_idx = 0; 

    // 3. 最后清除标志位 (相当于"开门"，允许新数据写入)
    cmd_ready = 0;
}

/**
 * @brief 串口中断回调函数 (重写 HAL 弱定义)
 * 职责：实现字节拼接、行结束符识别及缓冲区溢出保护
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        if (cmd_ready == 1) {
             // 重新开启接收并返回，相当于“拒接新客”
             HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
             return; 
        }
        // 识别行结束符 (\n 或 \r)
        if (rx_byte == '\n' || rx_byte == '\r') {
            rx_line[rx_idx] = '\0'; // 插入字符串结束符
            if (rx_idx > 0) {
                cmd_ready = 1;      // 标记命令已就绪
            }
            rx_idx = 0;             // 复位索引，准备下一行接收
        } 
        else if (rx_idx < (RX_BUFFER_SIZE - 1)) { // 防止缓冲区溢出
            rx_line[rx_idx++] = rx_byte;
        }
        // 重新开启下一次中断接收，形成闭环
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}
