/***************************************************************************
 *  scc.h
 *
 *  Created: Nov, 22 2011
 *  Changed: Dec, 17 2011
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
 * mai scc (serial boards on cmb) - Zilog z8530
 * see M8097C 'MAI 2000 Desktop Computer System Sevice Manual'
 */

#ifndef SCC_H
#define SCC_H

#include "sim.h"

#define SCC_ADDR		0x600000
#define BAUD_ADDR		0x640000
#define SCC_ADDR_MASK	0xFFFF0000
#define SCC_REG_MASK	0xFFFF000F
#define SCC_INTNO		5

#define ADDR_IS_SCC(ADDR) (((ADDR & SCC_ADDR_MASK) == SCC_ADDR) | ((ADDR & SCC_ADDR_MASK) == BAUD_ADDR))


/*	A02		A01		A00
	A//B	D//C
	0(B)	0(cmd)	0	0x600000 - B cmd
	0(B)	1(data)	0	0x600002 - B data
	1(A)	0(cmd)	0	0x600004 - A cmd
	1(A)	1(data)	0	0x600006 - A data
*/
#define SCC_B_CMD	0x600000
#define SCC_B_DATA	0x600002
#define SCC_A_CMD	0x600004
#define SCC_A_DATA	0x600006

unsigned int scc_read_byte(unsigned int address);
unsigned int scc_read_word(unsigned int address);
void scc_write_byte(unsigned int address, unsigned int value);
void scc_write_word(unsigned int address, unsigned int value);
void scc_pollStatus (void);
void scc_pulse_reset(void);
int scc_dbgCmd(int numArgs, struct args_t * args);

int scc_save_state(FILE * f);
int scc_load_state(FILE * f);

#endif
