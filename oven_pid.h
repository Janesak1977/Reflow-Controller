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

#ifndef OVEN_PID_H_INCLUDED
#define OVEN_PID_H_INCLUDED

#include <stdint.h>

#define EE_PID_Params_Adr 00    // Address of variable PID_Params(1 if PIDs coefs are valid in EEPROM, 0 if PIDs are not valid in EEPROM)
#define EE_PID_P_Adr  01
#define EE_PID_I_Adr  02
#define EE_PID_D_Adr  03

// constants
extern volatile uint8_t k_p;
extern volatile uint8_t k_i;
extern volatile uint8_t k_d;

int32_t command;
volatile int16_t error;
volatile int16_t derivative;
volatile int32_t pid_int; // integral

void WritePIDsCoefs(uint8_t, uint8_t, uint8_t);
void ReadPIDCoefs(void);
void pid_reset(void);
uint8_t pid_update(int16_t, int16_t);

#endif

