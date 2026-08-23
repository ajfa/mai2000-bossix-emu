/***************************************************************************
 *   cmb.h
 *
 *  Tue November 22 21:23:11 2011
 *  Copyright  2011  Armin Diehl
 *  <ad@ardiehl.de>
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
 * mai 2000 cmb definitions
 * see M8097 'MAI 2000 Desktop Computer System Sevice Manual'
 */

#ifndef CMB_H
#define CMB_H

#define CMB_ADDR			0x200000
#define CMB_ADDR_MASK		0xFFF00000
#define ADDR_IS_CMB(ADDR) ((ADDR & CMB_ADDR_MASK) == CMB_ADDR)
#define CMB_ADDR_REG_MASK	0x0F
#define CMBW_TSTOL			0x0		/* Write: Test point TSTOL */
#define CMBW_TSTERL			0x2		/* Write: Test point TSTERL */
#define CMBW_INHPAR			0x4		/* Write: Inhibit parallel port drivers */
#define CMBW_PARDATA		0x6		/* Write: Parity data forced */
#define CMBW_INHSER			0x8		/* Write: Inhibit serial port drivers */
#define CMBW_LED			0xA		/* Write: Turn on LED */
#define CMBW_INHPARITY		0xC		/* Write: Inhibit parity response */
#define CMBR_MEMPAR_HI		0x2		/* Read: Memory parity error upper register */
#define CMBR_MEMPAR_LO		0x4		/* Read: Memory parity error lower register */
#define CMBR_STATUS			0xA		/* Read: CMB status register */
#define CMBR_GENSTATUS		0xE		/* Read: CMB general status register */
#define CMB_REGADDR_MASK	0xFFF0000F

/* bits for status register in emulator */
#define CMBWSTAT_TSTOL			0x01		/* Write: Test point TSTOL */
#define CMBWSTAT_TSTERL			0x02		/* Write: Test point TSTERL */
#define CMBWSTAT_INHPAR			0x04		/* Write: Inhibit parallel port drivers */
#define CMBWSTAT_PARDATA		0x08		/* Write: Parity data forced */
#define CMBWSTAT_INHSER			0x10		/* Write: Inhibit serial port drivers */
#define CMBWSTAT_LED			0x20		/* Write: Turn on LED */
#define CMBWSTAT_INHPARITY		0x40		/* Write: Inhibit parity response */

#define CMB_ADDR_ROM			0x400000
#define CMB_ADDR_ROM_MASK		0xFFF00000

/* CMBR_STATUS register bits */
#define CMBR_STATUS_BIT_MMERF	0	/* MMU err flag */
#define CMBR_STATUS_BIT_PARITY	1	/* parity error flag */
#define CMBR_STATUS_BIT_RTCZERO	2	/* hw timer count 0 int */
#define CMBR_STATUS_BIT_PFD		3	/* power fail */
#define CMBR_STATUS_BIT_HI1		4
#define CMBR_STATUS_BIT_LO1		5
#define CMBR_STATUS_BIT_BUTTON	6
#define CMBR_STATUS_BIT_SSWARNF	7	/* MMU stack overflow */
#define CMBR_STATUS_BIT_RINGA	8
#define CMBR_STATUS_BIT_DSRA	9	/* port A data available */
#define CMBR_STATUS_BIT_LO2		10
#define CMBR_STATUS_BIT_RINGB	11
#define CMBR_STATUS_BIT_DSRB	12
#define CMBR_STATUS_BIT_HI2		13
#define CMBR_STATUS_BIT_LO3		14
#define CMBR_STATUS_BIT_LO4		15



unsigned int cmb_read_byte(unsigned int address);
unsigned int cmb_read_word(unsigned int address);
unsigned int cmb_read_long(unsigned int address);
void cmb_write_byte(unsigned int address, unsigned int value);
void cmb_write_word(unsigned int address, unsigned int value);
void cmb_write_long(unsigned int address, unsigned int value);
void cmb_pulse_reset(void);
unsigned int cmb_getStatusWord (void);
unsigned int cmb_getWriteRegs (void);
int cmb_dbgCmd(int numArgs, struct args_t * args);

#endif
