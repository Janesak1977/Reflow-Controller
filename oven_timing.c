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
#include <stdint.h>
#include "oven_timing.h"
#include "oven_ssr.h"
#include "pinassig.h"

#define buzzer_on() PORTD |= _BV(SPKR)
#define buzzer_off() PORTD &= ~_BV(SPKR)

extern uint8_t  buzzer;

// implemented in main program (ovencon.c)
extern void oven_update_4hz(void);

uint8_t div = 0;
uint8_t divbut=3;
uint8_t ButtTmp = 0xFF;
uint8_t ButtTmp2 = 0xFF;

void timing_setup(void)
{
    div     = 0;

    // CTC with ICRn TOP, clk/8
	// ICR1    = 10101; // 8MHz/(8*10101) = 99Hz (slightly under mains frequency; will sync with external interrupt)
    ICR1    = 10101; // 99 Hz with external sync
    TCCR1A  = 0;
    TCCR1B  = _BV(WGM13) | _BV(WGM12) | _BV(CS11);
    TCCR1C  = 0;
    OCR1A   = 9000; // update outputs a little while before next zero-cross
    TIMSK1  = _BV(OCIE1A); // enable OCRA1 interrupt

    // enable external interrupt (INT0/PD0; falling edge)
 
    DDRD    &= ~(_BV(2));
    PORTD   |= _BV(2); // pull-up
    EICRA   = _BV(ISC01);
    EIMSK   = _BV(INT0);
}

// timer interrupt
ISR(TIMER1_COMPA_vect)
{
    ssr_update();

    // re-enable interrupts
    sei();

	if (--divbut==0)
	{
		ButtTmp = ButtTmp2;
		ButtTmp2 = PIND & 0xC0;		//Read buttons every 30ms
		if ((bit_is_clear(ButtTmp,GButton)) && (bit_is_set(PIND,GButton))) 		//G Button pressed and released	
			Buttons |= 1<<GButton;
		if ((bit_is_clear(ButtTmp,RButton)) && (bit_is_set(PIND,RButton))) 		//G Button pressed and released	
			Buttons |= 1<<RButton;
		divbut=3;
	}


    // execute control update every 25 steps (100/25 = 4 Hz)
    if(++div == 25)
    {
        div = 0;
        oven_update_4hz();

		if (buzzer != 0)
		{
			if (buzzer % 2 == 0)
				buzzer_on();
			else
				buzzer_off();
				
			buzzer--;
		}
    }
}

// AC zero-cross interrupts - just clear timer
ISR(INT0_vect)
{ 
	TCNT1 = 0;
}



