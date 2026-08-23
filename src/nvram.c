/***************************************************************************
 *   nvram.c
 *
 *  Created:	Nov 27,2011
 *  Changed:	Dec 21,2011 
 *  Armin Diehl <ad@ardiehl.de>
 ****************************************************************************/
/*
 * 2000 nvram
 * There is a Xicor X2210 (64x4 nvram/static ram) installed. The chip is a
 * ram and eeprom. Contents of ram will be saved to the internal eeprom by a 
 * supervisor write (not read as stated in the manuals) to 0x6Axxxx. Recall 
 * from internal eeprom to ram will be done by a supervirsor write to 0x6Cxxxx.
 * see M8097C 'MAI 2000 Desktop Computer System Sevice Manual', Section 3.2.9
 * 
 * diags nvram test: no errors
 */

#include <stdio.h>
#include <string.h>
#include "m68k.h"
#include "nvram.h"
#include "sim.h"

char nvram[NV_SIZE];	/* only lower 4 bits are used */

void nvram_save() {
	FILE* fhandle;

	if((fhandle = fopen(NV_FILENAME, "wb")) == NULL) {
		msgout (MSGC_FATAL,MSG_NV,MSG_SAVE,"unable to create contents file %s",NV_FILENAME);
		return;
	}
	if(fwrite(nvram, 1, NV_SIZE, fhandle) <= 0) { 
		fclose(fhandle);
		msgout (MSGC_FATAL,MSG_NV,MSG_SAVE,"unable to write contents to %s",NV_FILENAME);
		return; 
	}
	fclose(fhandle);
	msgout (MSGC_WARN,MSG_NV,MSG_SAVE,"content saved to %s",NV_FILENAME);
}


void nvram_load() {
	FILE* fhandle;

	if((fhandle = fopen(NV_FILENAME, "rb")) == NULL) {
		msgout (MSGC_ERR,MSG_NV,MSG_LOAD,"unable to open contents file %s",NV_FILENAME);
		return;
	}
	if(fread(nvram, 1, NV_SIZE, fhandle) <= 0) { 
		fclose(fhandle);
		msgout (MSGC_ERR,MSG_NV,MSG_LOAD,"unable to read contents from %s",NV_FILENAME);
		return; 
	}
	fclose(fhandle);
	msgout (MSGC_WARN,MSG_NV,MSG_LOAD,": content restored fom %s",NV_FILENAME);
}


unsigned int nv_read_byte(unsigned int address) {
	int idx;
	
	if (ADDR_IS_NV_BANK0(address)) {
		if (address & 1) { msgout (MSGC_ERR,MSG_NV,MSG_READB,"access to invalid(odd) bank0 address %08x",address); return 0xff; }
		idx = (address >> 1) & 0xff;
		msgout (MSGC_INFO,MSG_NV,MSG_READB,"%02x from bank0 %08x, idx:%d",nvram[idx] & 0x0f,address,idx);
		return nvram[idx] & 0x0f;
	} else
	if (ADDR_IS_NV_BANK1(address)) {
		if (address & 1) { msgout (MSGC_ERR,MSG_NV,MSG_READB,"nv: access to invalid(odd) bank1 address %08x",address); return 0xff; }
		idx = ((address & 0xff) >> 1) & 0xff;
		msgout (MSGC_INFO,MSG_NV,MSG_READB,"%02x from bank1 %08x, idx in bank1:%d",nvram[idx+NV_BANK_SIZE] & 0x0f,address,idx);
		return nvram[idx+NV_BANK_SIZE] & 0x0f;
	}
	/* not as stated in the manual, only on write
	if (ADDR_IS_NV_SAVE(address)) { nvram_save(); return 0xff; }
	if (ADDR_IS_NV_RECALL(address)) { nvram_load(); return 0xff; } */
	msgout (MSGC_ERR,MSG_NV,MSG_READB,"from unknown address %08x",address);
	return 0xff;
}

unsigned int nv_read_word(unsigned int address) {
	return ((nv_read_byte(address)<<8) | (nv_read_byte(address)));
}

void nv_write_byte(unsigned int address, unsigned int value) {
	int idx;
	
	if (ADDR_IS_NV_BANK0(address)) {
		if (address & 1) { msgout (MSGC_ERR,MSG_NV,MSG_WRITEB,"to invalid(odd) bank0 address %08x",address); return; }
		idx = (address >> 1) & 0xff;
		nvram[idx] = value & 0x0f;
		msgout (MSGC_INFO,MSG_NV,MSG_WRITEB,"%02x to bank0 %08x, idx:%d",value,address,idx);
	} else
	if (ADDR_IS_NV_BANK1(address)) {
		if (address & 1) { msgout (MSGC_ERR,MSG_NV,MSG_WRITEB,"to invalid(odd) bank1 address %08x",address); return; }
		idx = ((address & 0xff) >> 1) & 0xff;
		if ((NV_BANK1_PROTECTED) && (idx < NV_BANK1_PROTSIZE)) {
			msgout (MSGC_WARN,MSG_NV,MSG_WRITEB,"%02x to upper protected nvram bank %08x denied idx in bank1:%d",value,address,idx);
		} else {
			nvram[idx+NV_BANK_SIZE] = value & 0x0f;
			msgout (MSGC_INFO,MSG_NV,MSG_WRITEB,"%02x to bank1 %08x, idx in bank1:%d",value,address,idx);
		}
	} else
	/* docs state that addr is read only but rom does only writes (0x40351A) */
	if (ADDR_IS_NV_RECALL(address))  
		nvram_load(); 
	else
	if (ADDR_IS_NV_SAVE(address)) 
		nvram_save(); 
	 else  /* this should not happen: */
		msgout (MSGC_ERR,MSG_NV,MSG_WRITEB,"to unknown address %08x",address);
}

void nv_write_word(unsigned int address, unsigned int value) {

	if (!(address & 1)) {  /* 16 bit to even address, to avoid warning, ignore upper 8 bit */
		nv_write_byte(address,(value >> 8) & 0xff);
	} else
	if (ADDR_IS_NV_RECALL(address))  
		nvram_load(); 
	else
	if (ADDR_IS_NV_SAVE(address)) 
		nvram_save(); 
	else
		msgout (MSGC_ERR,MSG_NV,MSG_WRITEW,"to odd address %08x not supported",address);
}

void nv_pulse_reset (void) {
	memset(nvram,0x00,sizeof(nvram));
}

int nv_save_state(FILE * f) {
    STATEWRITEVARS("nvr");

    STATEWRITE(id,f);
    STATEWRITELEN(nvram,f);
    return 1;
}

int nv_load_state(FILE * f) {
    STATEREADVARS("nvr");

    STATEREADID(f);
    STATEREADLEN(nvram,f);
    return 1;
}
