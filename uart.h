/* Filename: uart.h
 * Info: controls one uart
 *
 * Author: Johan Persson 2009
 */
#ifndef UART_H_
#define UART_H_

void uart_init(void);
void uart_sendChar(char data);
char uart_recvChar(void);
uint8_t uart_char_available(void);
void uart_sendBuff(char *data, uint8_t size);

#endif

