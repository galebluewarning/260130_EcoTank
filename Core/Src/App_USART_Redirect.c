#include "App_USART_Redirect.h"
#include "usart.h"

/* * 引入串口硬件句柄。
 * 如果你使用的是 UART2 或其他接口，请在这里修改为 huart2 等。
 */
extern UART_HandleTypeDef huart1;

/**
 * @brief 重写 C 标准库输出函数
 * 适配 GCC (STM32CubeIDE) 和 MDK (Keil) 环境
 */
#ifdef __GNUC__
/* GCC 环境下的重定向实现 */
int _write(int file, char *ptr, int len) {
    // 采用阻塞方式发送，确保数据完整输出
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
#else
/* Keil MDK 环境下的重定向实现 */
int fputc(int ch, FILE *f) {
    // 发送单个字符到串口
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
#endif
