/**
 * Copyright (c) 2011, Daniel Strother < http://danstrother.com/ >
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   - The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdint.h>

#include <string.h>
#include <stdio.h>


#include "oven_ssr.h"
#include "oven_timing.h"
#include "oven_pid.h"
#include "oven_profile.h"
#include "pinassig.h"
#include "max31855.h"
#include "lcd.h"
#include "uart.h"
#include "spi.h"


#define rled_on() PORTB |= _BV(RLED)
#define rled_off() PORTB &= ~_BV(RLED)

#define gled_on() PORTB |= _BV(GLED)
#define gled_off() PORTB &= ~_BV(GLED)

// commands/config to controller from serial

volatile uint8_t mode_manual;
volatile uint8_t manual_target_changed;

// TODO: there is a risk that some of these 16-bit values could be caught
// in a half-updated state, if the timer interrupt causes oven_update_4hz
// to be executed in the middle of process_message.
// Since these values are generally only used for testing, and not during
// actual reflow operations, this may not be a major concern.

volatile int16_t manual_target;


#define CMD_RESET	1
#define CMD_GO		2
#define CMD_PAUSE	3
#define CMD_RESUME	4
#define CMD_MANUAL	5

// TODO: currently, only one comm_cmd can be processed per oven_update_4hz
// invocation, so multiple commands received within a ~0.25s window may be lost
volatile uint8_t comm_cmd;


// controller state

#define ST_FAULT	0
#define ST_IDLE		1
#define ST_RUN		2
#define ST_DONE		3
#define ST_PAUSE	4
#define ST_MANUAL	5


//Tc error code
#define TCERR_SCV	0x0FFFD
#define TCERR_SCG	0x0FFFE
#define TCERR_OC	0x0FFFF

const char *ReflowStates[7] = { "RampTemp","PreHeat","Soak","Reflow","Cool" };
const char *state_names[6] = { "Fault   ","Idle    ","Run     ","Done    ","Pause   ", "Manual  " };

uint8_t state = ST_FAULT;

int16_t		target;
uint16_t	time;
uint16_t	TtoTarget;
uint8_t		buzzer;
int16_t		CJTemp;
double		TempCelsius;

uint8_t lcd_update_state = 0;
uint8_t lcd_update_CurrTemp = 0;
uint8_t lcd_update_2nd_row = 0;
uint8_t send_status = 0;

volatile uint8_t k_p = 0;
volatile uint8_t k_i = 0;
volatile uint8_t k_d = 0;



void fault(void)
{
	state = ST_FAULT;
	ssr_fault();
	lcd_update_state = 1;
	lcd_update_CurrTemp = 1;
	lcd_update_2nd_row = 1;
}

void oven_output(uint8_t top)
{
	ssr_set(top);
}

void oven_input(int16_t *top)
{
	*top = max31855_read(&CJTemp);
}

void oven_setup(void)
{
	state = ST_IDLE;

	mode_manual = 0;
	manual_target_changed = 0;
	manual_target = 0;

	comm_cmd = 0;

	target = 0;
	time = 0;
	TtoTarget = 0;
	buzzer = 0;
	Buttons = 0;

	// Setup PID coefs from EEPROM or set default values
	if (eeprom_read_byte(EE_PID_Params_Adr) == 1)
	{
		ReadPIDCoefs();
	}
	else
	{
		k_p = 23;	//actualy 2.3 (because dividing command/k_div)
		k_i = 0;
		k_d = 0;
	}

	pid_reset();
	profile_reset(0);
	ssr_setup();
	SPI_MasterInit();
	max31855_setup();
	uart_init();
	timing_setup(); // timing setup last, since it enables timer interrupts (which invoke the update functions below)
}


char tx_msg[50];
uint8_t profile_num = 0;
uint8_t HeaterPwr=0;
volatile uint8_t tx_len = 0;
volatile uint8_t lcd_update_1hz = 0;
volatile uint8_t led_blink = 0;
int16_t temp = 0;
uint8_t step;

void oven_update_4hz(void)
{
	oven_input(&temp);

	switch (temp)
	{
		case 0x0FFFF:
		case 0x0FFFE:
		case 0x0FFFD:
			if (state!=ST_FAULT)
				fault();
			
			break;
	}

	lcd_update_CurrTemp = 1;

	if (state==ST_RUN)
	{
		lcd_update_2nd_row = 1;

		if(profile_update(&target, &step, &TtoTarget, &buzzer))
		{
			state = ST_DONE;
			buzzer = 8;
			lcd_update_state = 1;
		}
	}

	send_status = 1;


	HeaterPwr = pid_update(temp,target);
	oven_output(HeaterPwr);

	if (led_blink==1)
		PORTB ^= (1<<RLED);

	time++;
}

char rx_msg[50];
uint8_t rx_cnt;

void process_message(const char *msg)
{
	// this is a ridiculously expensive function to invoke - a more efficient
	// command parser could be implemented, or a binary protocol established -
	// but, we're not expecting a lot of command traffic in this application,
	// and all of the timing-critical routines are handled by interrupts, so
	// there isn't a lot of downside to this expensive-but-easy implementation

	char tmp;

	tmp = sscanf_P(msg,PSTR("coefs: %d %d %d"),&k_p, &k_i, &k_d);
	if (tmp ==3)
	{
		WritePIDsCoefs(k_p, k_i, k_d);
		tx_len = sprintf_P(tx_msg,PSTR("* Coefs: %d %d %d\n"), k_p, k_i, k_d);

	}

	if (strcmp_P(msg,PSTR("coefs?")) == 0)
	{
		tx_len = sprintf_P(tx_msg,PSTR("* Coefs: %d %d %d\n"), k_p, k_i, k_d);
	}

}

char tmp;
int16_t DbgTemp, DbgTarget;

// program entry point
int main(void)
{
	char c;


	// Pullups for buttons on PORTD
	PIND |= (1<<RButton)|(1<<GButton);

	// LED status lights
	DDRB |= (1<<RLED); 
	DDRB |= (1<<GLED);

	// Buzzer
	DDRB |= (1<<SPKR);

	k_p = 23;
	k_i = 0;
	k_d = 0;

	DbgTemp = 92;
	DbgTarget = 600;	//debug 23C,150C

	pid_reset();

	// initialize LCD
	lcd_init(LCD_DISP_ON);
	lcd_clrscr();

	lcd_puts("REFLOW  OVEN");
	lcd_gotoxy(0, 1);
	lcd_puts(" CONTROLLER ");
	_delay_ms(1000);
	
	lcd_clrscr();
	lcd_puts("FIRMWARE VERSION");
	lcd_gotoxy(0, 1);
	lcd_puts("1.0");

	_delay_ms(2000);

	// initialize
	oven_setup();


	rx_cnt = 0;
	char lcd_str[16];
	char lcd_degree = 0xDF;

	lcd_update_state = 1;
	lcd_update_CurrTemp = 1;
	lcd_update_2nd_row = 1;





	// ******************** MAIN LOOP ***********************
	while(1)
	{
		if(comm_cmd != 0)
		{
			switch(comm_cmd)
			{
				case CMD_RESET:
					profile_reset(0);
					pid_reset();
					led_blink=0;
					target = 0;
					step = 0;
					temp = 0;
					state = ST_IDLE;
					lcd_update_state = 1;
					lcd_update_CurrTemp = 1;
					lcd_update_2nd_row = 1;
				break;

				case CMD_GO:
					if(state == ST_IDLE)
					{
						state = ST_RUN;
						lcd_update_state = 1;
						lcd_update_2nd_row = 1;
					}
				break;

				case CMD_PAUSE:
					if(state == ST_RUN)
					{
						state = ST_PAUSE;
						lcd_update_state = 1;
						lcd_update_2nd_row = 1;
					}

				break;

				case CMD_RESUME:
					if(state == ST_PAUSE)
					{
						state = ST_RUN;
						lcd_update_state = 1;
						lcd_update_2nd_row = 1;
					}
				break;

				case CMD_MANUAL:
				if (state == ST_IDLE)
				{
					target = manual_target;
					state = ST_MANUAL;
					lcd_update_state = 1;
					lcd_update_2nd_row = 1;
				}
				break;

				default:
					fault();
			}	//end switch(comm_cmd)

			comm_cmd = 0;
		}	//end if(comm_cmd != 0)


		switch(state)
		{
			case ST_IDLE:
				rled_off();
			break;

			case ST_RUN:
				rled_on();
				//if ((target - temp) > 100) // 25 *C difference
				//	state = ST_FAULT;
			break;

			case ST_PAUSE:
				rled_on();
				// hold target
			break;

			case ST_DONE:
				rled_off();
				target = 0;
			break;

			case ST_MANUAL:
				rled_on();
			break;

			case ST_FAULT:
				led_blink=1;
			break;

			default:
				fault();
		}

		if (bit_is_set(Buttons, RButton)) // button 1 (red) pressed
		{
			switch (state)
			{
				case ST_FAULT:
					mode_manual = 0;
					comm_cmd = CMD_RESET;
				break;

				case ST_IDLE:
					if (!mode_manual)
						comm_cmd = CMD_GO;
					else
						comm_cmd = CMD_MANUAL;
				break;

				case ST_RUN:
					comm_cmd = CMD_PAUSE;
				break;

				case ST_DONE:
					comm_cmd = CMD_RESET;
				break;

				case ST_PAUSE:
					comm_cmd = CMD_RESET;
				break;

				case ST_MANUAL:
					if (!manual_target_changed)
						comm_cmd = CMD_PAUSE;
					else
					{
						target = manual_target;
						manual_target_changed = 0;
						lcd_update_2nd_row = 1;
					}
				break;

				default:
					break;
			}
		Buttons &= ~(1<<RButton);
		}

		if (bit_is_set(Buttons, GButton)) // button 2 (green) pressed
		{
			switch (state)
			{
				case ST_FAULT:
				break;

				case ST_IDLE:
					profile_num++;
					mode_manual = 0;
					if (profile_num == (PROFILES+1))
						profile_num = 0;
					if (profile_num < PROFILES)
						profile_reset(profile_num);
					if (profile_num == PROFILES)
					{
						mode_manual = 1;
						manual_target = 600; // 150 *C
					}
					lcd_update_2nd_row= 1; 
				break;

				case ST_RUN:
				break;

				case ST_DONE:
				break;

				case ST_PAUSE:
					comm_cmd = CMD_RESUME;
				break;

				case ST_MANUAL:
					manual_target += 20;
					if (manual_target > 1000) // 250 *C
						manual_target = 200; // 50 *C
					manual_target_changed = 1;
					lcd_update_2nd_row = 1;
				break;

				default:
				break;
			}
			Buttons &= ~(1<<GButton);
		}


		if (lcd_update_CurrTemp == 1)
		{
			lcd_gotoxy(8, 0);
			if (state==ST_FAULT)
				sprintf_P(lcd_str, PSTR("--%cC        "), lcd_degree);
			else
				sprintf_P(lcd_str, PSTR("%d%cC        "), temp>>2, lcd_degree);
			lcd_puts(lcd_str);
			lcd_update_CurrTemp = 0;
		}


		if (lcd_update_state == 1)
		{
			lcd_gotoxy(0, 0);
			sprintf_P(lcd_str, PSTR("%s"), state_names[state]);
			lcd_puts(lcd_str);
			lcd_update_state = 0;
		}


		if (lcd_update_2nd_row == 1)
		{
			lcd_gotoxy(0, 1);

			if (state == ST_IDLE)
			{
				switch (profile_num)
				{
					case 0:
						lcd_puts("1.Lead          ");
						break;
					case 1:
						lcd_puts("2.Lead (KESTER) ");
						break;
					case 2:
						lcd_puts("3.Lead-free     ");
						break;
					case 3:
						lcd_puts("4.Manual control");
						break;
					default:
						lcd_puts("UNKNOWN PROFILE ");
						break;
				}
			}

			if (state == ST_FAULT)
			{
				switch(temp)
				{
					case 0xFFFF:
						lcd_puts("TH.COUPLE OPEN! ");
						break;
					case 0xFFFE:
						lcd_puts("TH.COUPLE SHRT G");
						break;
					case 0xFFFD:
						lcd_puts("TH.COUPLE SHRT +");
						break;
				}
			}

			if (state == ST_PAUSE)
			{
				lcd_puts("R stop, G resume");
			}

			if (state == ST_MANUAL)
			{
				if (manual_target_changed == 1)
				{
					sprintf_P(lcd_str, PSTR("%d%cC->%d%cC?     "), target>>2, lcd_degree, manual_target>>2, lcd_degree);
					lcd_puts(lcd_str);
				}
				else
				{
					sprintf_P(lcd_str, PSTR("Target: %d%cC     "), target>>2, lcd_degree);
					lcd_puts(lcd_str);
				}
			}

			if (state == ST_RUN)
			{
				lcd_gotoxy(0, 1);
				sprintf_P(lcd_str, PSTR("%d/%d %ds %d%cC     "), step, STEPS, TtoTarget, target>>2, lcd_degree);
				lcd_puts(lcd_str);
			}

			lcd_update_2nd_row = 0;
		}	//end if (lcd_update_2nd_row == 1)


		if (send_status == 1)
		{
			if (state == ST_RUN)
			{
				if(tx_len == 0)
				{
					tx_len = sprintf_P(tx_msg,PSTR("%s,%u,%u,%d,%u,%d,%u\n"),
					state_names[state],
					step,
					time,
					temp,
					TtoTarget,
					target,
					HeaterPwr);

					send_status = 0;
				}
			}

			if (state == ST_MANUAL)
			{
				if(tx_len == 0)
				{
					tx_len = sprintf_P(tx_msg,PSTR("%u,%d,%d,%u,%d,%d,%d\n"),
					time,
					temp,
					target,
					HeaterPwr,
					error,			// PID Error
					derivative,		// PID derivative
					pid_int);		// PID integrate

					send_status = 0;
				}
			}
		}


		// if the control loop has generated a status update message,
		// send it out over serial port to the host
		if(tx_len != 0)
		{
			uart_sendBuff((char*)tx_msg,tx_len);
			tx_len = 0; // clear the length, so the control loop knows it can generate a new message
		}

		if (state==ST_IDLE)
		{ 

			// receive individual characters from the host
			while( uart_char_available() != 0)
			{
				// all commands are terminated with a new-line
				c = uart_recvChar();
				if(c == '\n')
				{
					// only process commands that haven't overflowed the buffer
					if(rx_cnt > 0 && rx_cnt < 50)
					{
						
						rx_msg[rx_cnt] = '\0';
						process_message(rx_msg);
					}

					rx_cnt = 0;
				}
				else
				{
					// buffer received characters
					rx_msg[rx_cnt] = c;
					if(rx_cnt != 50) rx_cnt++;
				}
			}
		}
	}
}
