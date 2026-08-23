/***************************************************************************
 *  pit.h
 *
 *  Created: Dec, 12 2011
 *  Changed: Dec, 13 2011
 *  Armin Diehl <ad@ardiehl.de>
 ****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * mai 2000 pit (parallel interface timer) - MC 68230 emulation
 */

#ifndef PIT_H
#define PIT_H

#include "sim.h"

/* auto vectored interrupts for parport and timer */
#define PIT_PINTR			1
#define PIT_TINTR			6

#define PIT_ADDR			0x620000
#define PIT_ADDR_MASK		0xFFFF0000
#define PIT_ADDR_REGMASK	0x00000FF0	

#define ADDR_IS_PIT(ADDR) ((ADDR & PIT_ADDR_MASK) == PIT_ADDR)

#define PIT_GETREGNUM(A)	((A & PIT_ADDR_REGMASK)>>4)
#define PIT_MAX_REGISTERS	32


unsigned int pit_read_byte(unsigned int address, int flags);
unsigned int pit_read_word(unsigned int address, int flags);
void pit_write_byte(unsigned int address, unsigned int value, int flags);
void pit_write_word(unsigned int address, unsigned int value, int flags);
void pit_pulse_reset(void);
void pit_check_interrupt(void);
void pit_pulse_counter(void);
int  pit_irq_ack(int level);
void pit_update_irq(void);
int pit_dbgCmd(int numArgs, struct args_t * args);

int pit_save_state(FILE * f);
int pit_load_state(FILE * f);


#endif
