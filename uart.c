/* Filename: uart.c
 * Info: controls one uart
 *
 * Author: Johan Persson 2009
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include "pinassig.h"

#include "uart.h"

//UART0 OPTIONS
#define UART0_BAUD_RATE     38400

//interrupt control         //enable uart0
#define UART0_UCSRB_DATA    (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0)

//communication control     //8,E,1 
#define UART0_UCSRC_DATA    (1<<UPM01)|(1<<UCSZ01)|(1<<UCSZ00)

//data register empty
#define UART0_UDRE0         (1<<UDRE0)

//UART0 register
#define UART0_DATA          UDR0
#define UART0_UCSRA         UCSR0A
#define UART0_UCSRB         UCSR0B
#define UART0_UCSRC         UCSR0C
#define UART0_UBRR_HIGH     UBRR0H
#define UART0_UBRR_LOW      UBRR0L
#define UART0_INT_RECEIVE   SIG_USART_RECV

//calc baud rate 
#define UART0_UBRR_DATA     ((F_CPU / (16L * UART0_BAUD_RATE))-1)

//ring buffer size
#define BUFFER_SIZE 16

uint8_t uart_ring_buf[BUFFER_SIZE];
volatile uint8_t buf_in;
volatile uint8_t buf_out;


/* uart_init()
 * Info: Init uart0
 */
void uart_init(void){

  buf_in = 0;
  buf_out = 0;

  //set baud rate
  UART0_UBRR_HIGH = UART0_UBRR_DATA >> 8;
  UART0_UBRR_LOW  = UART0_UBRR_DATA;

  //set control and status register B
  UART0_UCSRB = UART0_UCSRB_DATA;
  
  //set control and status register C
   UART0_UCSRC = UART0_UCSRC_DATA;

  //enable interrupt
  sei();

}


/* uart_senddata()
 * info: puts data in register to be sent
 */
void uart_sendChar(char data){
  
  //wait for transmit buffer to be ready
  while(!(UART0_UCSRA & UART0_UDRE0));
  
  //send data
  UART0_DATA = data;
  
}

/* uart_recvchar()
 * Info: gets a received char(byte) from buffer
 */
char uart_recvChar(void){

  char c;
  
  //wait for new data
  while(buf_in == buf_out);
  
  //get data and decrease buf_out counter
  c = uart_ring_buf[buf_out];
  buf_out = (buf_out + 1) % BUFFER_SIZE;
  
  return c;
}



void uart_sendBuff(char *data, uint8_t size)
{
    while(size--)
	{
	   	uart_sendChar(*(data++));
  	}
  
}

uint8_t uart_char_available(void)
{
	if (buf_in == buf_out)
		return 0;
	else
		return -1;
}
  

/*
 * Info: UART receive interrupt
 */
SIGNAL(UART0_INT_RECEIVE)
{
  uart_ring_buf[buf_in] = UART0_DATA;
  buf_in = (buf_in + 1) % BUFFER_SIZE;

}

