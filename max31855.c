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
#include <util/delay.h>
#include "max31855.h"
#include "spi.h"

extern void fault(void);

#define AVERAGE_BITS 2
#define AVERAGE (1<<AVERAGE_BITS)


int16_t temps[AVERAGE];

static void _max31855_select(uint8_t cs)
{
            if(cs)  PORTD &= ~(_BV(MAX31855CS)); // active-low
            else    PORTD |= _BV(MAX31855CS);
            DDRD |= _BV(MAX31855CS);
}

void max31855_setup(void)
{
    uint8_t j;

    // set chip-selects inactive and initialize averages
        _max31855_select(0);
        for(j=0;j<AVERAGE;++j)
            temps[j] = 100; // 25C


    // start initial conversion
        _delay_us(100);
        _max31855_select(1);
        _delay_us(100);
        _max31855_select(0);
}

int16_t max31855_read(int16_t *CJTemp)
{ 
    int16_t MSBData, LSBData;
	int16_t result = 0;
	uint8_t TcoupleErr = 0;
    int16_t avg;
    uint8_t i;


    // select device
    _max31855_select(1);

    // read MSbyte
    SPDR = 0xFF;
    loop_until_bit_is_set(SPSR,SPIF);
    MSBData = SPDR;
    MSBData <<= 8;

	// read 2nd byte
	SPDR = 0xFF;
    loop_until_bit_is_set(SPSR,SPIF);
    MSBData |= SPDR;

	// read 3rd byte
	SPDR = 0xFF;
    loop_until_bit_is_set(SPSR,SPIF);
    LSBData = SPDR;
    LSBData <<= 8;

    // read LSbyte
    SPDR = 0xFF;
    loop_until_bit_is_set(SPSR,SPIF);
    LSBData |= SPDR;
    
    // de-select device (starts new conversion)
    _max31855_select(0);
	
	if ((MSBData & 0x0001) == 1)
	{
		TcoupleErr = LSBData & 0x0007;
		switch (TcoupleErr)
		{
			case 0x01:
				result = 0xFFFF;
				break;
			case 0x02:
				result = 0xFFFE;
				break;
			case 0x04:
				result = 0xFFFD;
				break;
		}
	}

	if (TcoupleErr==0)
	{
    	result = MSBData >> 2;		// Tc temperatur in 0.25 degC
		*CJTemp = LSBData >> 4;		// ColdJuntion Temperatur in 0.0625 degC
	}

    // check range (5-300 degrees)
    //if( result < 10 || result > 1200) {
     //   result = 0x0FFF;
     //   fault();
    //}

    // average
    avg = result;
    for(i=1;i<AVERAGE;++i)
    {
        temps[i] = temps[i-1];
        avg += temps[i];
    }
    temps[0] = result;

    avg >>= AVERAGE_BITS;

    
	return result;
}

