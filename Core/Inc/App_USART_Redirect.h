#ifndef __APP_USART_REDIRECT_H__
#define __APP_USART_REDIRECT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>

/**
 * 注意：此模块无需显式初始化函数。
 * 只要将此文件包含在 main.c 中，并确保相应的 UART 句柄已初始化，
 * 即可直接在工程任何位置使用 printf。
 */

#ifdef __cplusplus
}
#endif

#endif /* __APP_USART_REDIRECT_H__ */