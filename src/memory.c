/***************************************************************************
 *  memory.c
 *
 *  Created: Nov, 22 2011
 *  Changed: Dec, 17 2011
 *  Armin Diehl <ad@ardiehl.de>
 ****************************************************************************/
/*
 * memory access for RAM and ROM
 * MMU not included here
 */

#include <stdio.h>
#include <string.h>
#include "m68k.h"
#include "memory.h"
#include "sim.h"

#define MYSELF MSG_MEM

UINT32 romMappedAfterReset = 1; /* rom is @ each 8k until first write to memory */
unsigned char rom[MEM_ROMSIZE];
unsigned char ram[MEM_SIZE];

int mem_isValidForRead (unsigned int address) {
    if (ADDR_IS_RAM(address)) return 1;
    if (ADDR_IS_ROM(address)) return 1;
    return 0;
}

int mem_isValidForWrite (unsigned int address) {
    if (ADDR_IS_RAM(address)) return 1;
    return 0;
}

unsigned int mem_read_byte(unsigned int address, int flags) {
  if (romMappedAfterReset) {
    return READ_BYTE(rom,address & (MEM_ROMSIZE-1));
  } else {
    if (ADDR_IS_RAM (address)) 
		  return READ_BYTE(ram,address);
	if (ADDR_IS_ROM (address)) return READ_BYTE(rom,address & (MEM_ROMSIZE-1));
/*
	if (ADDR_IS_RAMSPACE (address)) {
		BUSERROR(flags,address,MSG_READB);
		return 0xff;
	}
	msgout (MSGC_ERR,MSG_MEM,MSG_READB,"unknown memory area %08",address);
*/
    BUSERROR(flags,address,MSG_READB);
	return 0xff;
  }
}


unsigned int mem_read_word(unsigned int address, int flags) {
if (romMappedAfterReset) {
    return READ_WORD(rom,address & (MEM_ROMSIZE-1));
  } else {
    if (ADDR_IS_RAM (address)) return READ_WORD(ram,address);
	if (ADDR_IS_ROM (address)) return READ_WORD(rom,address & (MEM_ROMSIZE-1));
/*
	if (ADDR_IS_RAMSPACE (address)) {
		BUSERROR(flags,address,MSG_READW);
		return 0xffff;
	}
	msgout (MSGC_ERR,MSG_MEM,MSG_READW,"unknown memory area %08",address);
*/
    BUSERROR(flags,address,MSG_READW);
	return 0xffff;
  }
}


void mem_write_byte(unsigned int address, unsigned int value, int flags) {
  if (romMappedAfterReset) {
    msgout (MSGC_INFO,MSG_MEM,MSG_WRITEB,"mapping rom to %08x",MEM_ADDR_ROM);
	romMappedAfterReset = 0;
  }
  if (ADDR_IS_RAM (address)) { WRITE_BYTE(ram,address,value); return; }
/*
  if (ADDR_IS_ROM (address)) { msgout (MSGC_ERR,MSG_MEM,MSG_WRITEB,"attempt to write %02x to rom @ %08x",value,address); return; }
  if (ADDR_IS_RAMSPACE (address)) { 
	  msgout (MSGC_ERR,MSG_MEM,MSG_WRITEB,"attempt to write %02x to non existent ram @ %08x",value,address);
	  BUSERROR(flags,address,MSG_WRITEB);
	  return; 
  }
  msgout (MSGC_ERR,MSG_MEM,MSG_WRITEB,"%02x to unknown memory %08x",value,address);
*/
  BUSERROR(flags,address,MSG_WRITEB);  
}

void mem_write_word(unsigned int address, unsigned int value, int flags) {
  if (romMappedAfterReset) {
    msgout (MSGC_INFO,MSG_MEM,MSG_WRITEW,"mapping rom to %08x",MEM_ADDR_ROM);
	romMappedAfterReset = 0;
  }
  if (ADDR_IS_RAM (address)) { WRITE_WORD(ram,address,value); return; }
/*
  if (ADDR_IS_ROM (address)) { msgout (MSGC_ERR,MSG_MEM,MSG_WRITEW,"attempt to write %04x to rom @ %08x",value,address); return; }
  if (ADDR_IS_RAMSPACE (address)) { 
	  msgout (MSGC_ERR,MSG_MEM,MSG_WRITEW,"attempt to write %04x to non existent ram @ %08x",value,address);
	  BUSERROR(flags,address,MSG_WRITEW);
	  return; 
  }
*/
  BUSERROR(flags,address,MSG_WRITEW);
//  msgout (MSGC_ERR,MSG_MEM,MSG_WRITEW,"%04x to unknown memory %08x",value,address); 
}


/* for setting ROM to 0x0 */
void mem_pulse_reset(void) {
  romMappedAfterReset = 1;
  memset(ram,0xff,sizeof(ram));
}

int loadROM (char * filename) {
	FILE* fhandle;

	if((fhandle = fopen(filename, "rb")) == NULL) return 1;
	if(fread(rom, 1, MEM_ROMSIZE, fhandle) <= 0) { fclose(fhandle); return 1; }
	fclose(fhandle);
	return 0;
}


int mem_save_state(FILE * f) {
    STATEWRITEVARS("mem");

    STATEWRITE(id,f);
    STATEWRITELEN(ram,f);
    STATEWRITELEN(romMappedAfterReset,f);
    return 1;
}

int mem_load_state(FILE * f) {
    STATEREADVARS("mem");

    STATEREADID(f);
    STATEREADLEN(ram,f);
    STATEREADLEN(romMappedAfterReset,f);
    return 1;
}

