/***************************************************************************
 *  nvram.h
 *
 *  Created:	Nov 27,2011
 *  Changed:	Dec 21,2011 
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
 * There is a Xicor X2210 (64x4 nvram/static ram) installed. The chip is a
 * ram and eeprom. Contents of ram will be saved to the internal eeprom by a 
 * supervisor read to 0x6Axxxx. Recall from internal eeprom to ram will be 
 * done by a supervirsor read to 0x6Cxxxx.
 * This is from the technical manual, however, there seems to be a large nvram
 * installed, the rom verifies access to 680120. The chip only contains a
 * MAI part numer, so i assume it is a 128 or 256x4 one
 *
 * Lower 32 nibbles are documented and contains boot params as terminal port, 
 * boot device, ...
 * Upper 32 nibbles contains the serial number information and are not writable
 * unless the address decoding pal is replaced by a 'ssn' pal. There was a
 * special ssn program available to field services allowing rewrite of the
 * serial number (later versions included a verfication code, MAI had to be
 * called for this serial number dependend verification code)
 * 
 * see M8097C 'MAI 2000 Desktop Computer System Sevice Manual', Section 3.2.9
 */

#ifndef NVRAM_H
#define NVRAM_H

#define NV_BANK1_PROTECTED	1
#define NV_ADDR				0x680000
#define NV_ADDR_SAVE		0x6A0000
#define NV_ADDR_RECALL		0x6C0000
#define NV_ADDR_MASK		0xFFFF0000
#define NV_BANK_MASK		0xFFFF0FFF
#define NV_SIZE				64
#define NV_BANK_SIZE		32
#define NV_BANK1_PROTSIZE	16
#define NV_FILENAME			"nvram.dat"


#define NV_ADDR_BANK0_MIN	NV_ADDR
#define NV_ADDR_BANK0_MAX	NV_ADDR+0x3E
#define NV_ADDR_BANK1_MIN	NV_ADDR+0x100
#define NV_ADDR_BANK1_MAX	NV_ADDR+0x13E

#define ADDR_IS_NV(ADDR) (((ADDR & NV_ADDR_MASK) == NV_ADDR) || ((ADDR & NV_ADDR_MASK) == NV_ADDR_SAVE) || ((ADDR & NV_ADDR_MASK) == NV_ADDR_RECALL))
#define ADDR_IS_NV_SAVE(ADDR) ((ADDR & NV_ADDR_MASK) == NV_ADDR_SAVE)
#define ADDR_IS_NV_RECALL(ADDR) ((ADDR & NV_ADDR_MASK) == NV_ADDR_RECALL)
#define ADDR_IS_NV_BANK0(ADDR) (((ADDR & NV_BANK_MASK) >= NV_ADDR_BANK0_MIN) && ((ADDR & NV_BANK_MASK) <= NV_ADDR_BANK0_MAX))
#define ADDR_IS_NV_BANK1(ADDR) (((ADDR & NV_BANK_MASK) >= NV_ADDR_BANK1_MIN) && ((ADDR & NV_BANK_MASK) <= NV_ADDR_BANK1_MAX))

unsigned int nv_read_byte(unsigned int address);
unsigned int nv_read_word(unsigned int address);

void nv_write_byte(unsigned int address, unsigned int value);
void nv_write_word(unsigned int address, unsigned int value);
void nv_pulse_reset (void);

int nv_save_state(FILE * f);
int nv_load_state(FILE * f);



#endif
