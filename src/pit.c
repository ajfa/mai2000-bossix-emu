/***************************************************************************
 *  pit.c
 *
 *  Created: Dec, 12 2011
 *  Changed: Dec, 21 2011
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
 * only basic timer functionality for now
 */

#include <stdio.h>
#include "m68k.h"
#include "pit.h"
#include "sim.h"

#define MYSELF MSG_PIT

int pit_regs[PIT_MAX_REGISTERS];

#define PGCR 0
#define PSSR 1
#define PADDR 2
#define PBDDR 3
#define PCDDR 4
#define PIVR 5
#define PACR 6
#define PBCR 7
#define PADR 8
#define PBDR 9
#define PAAR 10
#define PBAR 11
#define PCAR 12
#define PSR 13
#define TCR 16
#define TIVR 17
#define CPRH 19
#define CPRM 20
#define CPRL 21
#define CNTRH 23
#define CNTRM 24
#define CNTRL 25
#define TSR 26
	
char pit_regNames[][25] = {
	"Port general control",
	"Port service request",
	"Port A DDR",
	"Port B DDR",
	"Port C DDR",
	"Port int vector",
	"Port A control",
	"Port B control",
	"Port A data",
	"Port B data",
	"Port A alternate",
	"Port B alternate",
	"Port C data",
	"Port status",
	"unused",
	"unused",
		
	"Timer control",
	"Timer int vector",
	"unused",
	"Counter preload H",
	"Counter preload M",
	"Counter preload L",
	"unused",
	"Count H",
	"Count M",
	"Count L",
	"Timer status",
	"unused",
	"unused",
	"unused",
	"unused",
	"unused",
	""};
UINT32 counterValue;
UINT32 counterPreloadValue;
int pitIntAsserted = 0;     /* current state of the request line */


int pitGetReg(int regNo) {
	switch (regNo) {
		case CPRH: return ((counterPreloadValue >> 16) & 0xff);
		case CPRM: return ((counterPreloadValue >> 8) & 0xff);
		case CPRL: return (counterPreloadValue & 0xff);
		case CNTRH: return ((counterValue >> 16) & 0xff);
		case CNTRM: return ((counterValue >> 8) & 0xff);
		case CNTRL: return (counterValue & 0xff);
		default: return pit_regs[regNo];
	}
}

void pitSetReg(int regNo, int value) {
	/*if (regNo == TCR) printf("TCR: %02x\n",value);*/
	switch (regNo) {
		case TCR:	if ( (!(pit_regs[TCR] & 0x01)) && (value & 0x01)) {	/* switched on ? */
						pit_regs[TCR] = value & 0xf7; /* bit3 always 0 */
						/* enabling the timer loads the counter from the preload
						 * register (68230, zero detect control = 0). Without
						 * this the kernel inherited whatever the boot PROM
						 * left behind, and the PROM's last test ran in
						 * rollover mode, so the counter sat at sixteen
						 * million and the first kernel tick was hours away. */
						if (!(value & 0x10)) counterValue = counterPreloadValue;
						pit_check_interrupt();
					} else
						pit_regs[TCR] = value & 0xf7; /* bit3 always 0 */ 
					pit_update_irq();
					break; 
		case TSR:	if (value & 0x01) pit_regs[TSR] &= ~0x01;  /* write one to clear ZDS */
					pit_update_irq();
					break;
		case CPRH: counterPreloadValue = (counterPreloadValue & 0x00ffff) | (value << 16); break;
		case CPRM: counterPreloadValue = (counterPreloadValue & 0xff00ff) | (value << 8); break;
		case CPRL: counterPreloadValue = (counterPreloadValue & 0xffff00) | value; break;
		case CNTRH: counterValue = (counterValue & 0x00ffff) | (value << 16); break;
		case CNTRM: counterValue = (counterValue & 0xff00ff) | (value << 8); break;
		case CNTRL: counterValue = (counterValue & 0xffff00) | value; break;
		default: pit_regs[regNo] = value;
	}
}

unsigned int pit_read_byte(unsigned int address, int flags) {
	int regNo;

	regNo = PIT_GETREGNUM(address);
	if (regNo < PIT_MAX_REGISTERS) {
		msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x Reg:%d (%s)",address,PIT_GETREGNUM(address),pit_regNames[regNo]);
		return pitGetReg(regNo);

	} else {
		msgout (MSGC_ERR,MYSELF,MSG_READB,"%08x Reg:%d",address,PIT_GETREGNUM(address));
		return 0xff;
	}
}

unsigned int pit_read_word(unsigned int address, int flags) {
	return pit_read_byte(address,flags);
}


void pit_write_byte(unsigned int address, unsigned int value, int flags) {
	int regNo;

	regNo = PIT_GETREGNUM(address);
	if (regNo < PIT_MAX_REGISTERS) {
		pitSetReg(regNo, value);
		msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"%02x to %08x Reg:%d (%s)",value,address,PIT_GETREGNUM(address),pit_regNames[regNo]);
	} else
		msgout (MSGC_ERR,MYSELF,MSG_WRITEB,"%02x to %08x Reg:%d",value,address,PIT_GETREGNUM(address));
	/*BUSERROR(flags,address,MSG_WRITEB);*/ 
}

void pit_write_word(unsigned int address, unsigned int value, int flags) {
	pit_write_byte(address,value & 0x00ff,flags);
}


void pit_pulse_reset(void) {
	memset(pit_regs,0,sizeof(pit_regs));
	pit_regs[5] = 0x0f;
	pit_regs[17] = 0x0f;
	counterValue = 0;
	counterPreloadValue = 0;
	pitIntAsserted = 0;
	m68k_set_int_line (PIT_TINTR, CLEAR_LINE);
}

/*          765 4 3 21 0
 * TCR: bc: 101 1 1 10 0
 * 		bd: 101 1 1 10 1
 *          --- : : -- :
 *           :  : : :  - 1=Enable
 *           :  : : ---- Clock control (10: PC2/TIN pin serves as a timer input and the prescaler is used.
 *           :  : ------ Unused, should always be read as zero ???
 *           :  -------- Zero Detect Control (1=Rollover, 0=Reload on zero)
 * 00x The dual-function pins PC3/TOUT and PC7/TIACK carry the port C function
 * 01x The dual-function pin PC3/TOUT carries the TOUT function. In the run state it is used as a
 *     square-wave output and is toggled on zero detect. 
 * 100 The dual-function pin PC3/TOUT carries the TOUT function. In the run or halt state it is used as a
 *     timer interrupt request output. The timer interrupt is disabled; thus, the pin is always three stated.
 *     The dual-function pin PC7/TIACK carries the TIACK function; however, since interrupt request is
 *     negated, the PI/T produces no response (i.e. no data or DTACK) to an asserted TIACK. Refer to
 *     5.1.3 Timer Interrupt Acknowledge Cycles for details.
 * 101 The dual-function pin PC3/TOUT carries the TOUT function and is used as a timer interrupt request
 * 	   output. The timer interrupt is enabled; thus, the pin is low when the timer ZDS status bit is
 *     one. The dual-function pin PC7/TIACK carries the TIACK function and is used as a timer interrupt
 *     acknowledge input. Refer to the 5.1.3 Timer Interrupt Acknowledge Cycles for details. This com-
 *     bination supports vectored timer interrupts.
 * 110 The dual-function pin PC3/TOUT carries the TOUT function. In the run or halt state it is used as a
 *     timer interrupt request output. The timer interrupt is disabled; thus, the pin is alwas threstated.
 *     The dual-function pin PC7/TIACK carries the PC7 function.
 * 111 The dual-function pin PC3/TOUT carries the TOUT function and is used as a timer interrupt re-
 *     quest output. The timer interrupt is enabled; thus, the pin is low when the timer ZDS status bit is
 *     one. The dual~function pin PC7/TIACK carries the PC7 function and autivectored interrupts are
 *     supported.
 * 
 * PC3/TOUT is connected to INT6
 */

/* The 68230 timer request is LEVEL sensitive. ZDS, bit 0 of the timer status
 * register, is set when the counter reaches zero and stays set until software
 * writes a one to it. While ZDS is set, the timer is enabled and the TOUT
 * control selects an interrupt mode, the request line is asserted.
 *
 * Modelling it as a forced edge, which is what this used to do, had two
 * consequences. The boot PROM writes a test pattern to the vector register and
 * runs the counter down from 1 as a register check, and that raised an
 * interrupt that could never be withdrawn. And nothing ever handed back the
 * programmed vector, so the BOSS/IX clock handler at vector d4 was never
 * entered and the kernel idled forever.
 */
void pit_update_irq(void) {
	int tcr = pit_regs[TCR];
	int toutcontrol = (tcr >> 5) & 0x07;
	int assertIt;

	assertIt = (tcr & 1) &&                       /* timer running          */
	           (pit_regs[TSR] & 1) &&             /* zero detect status set */
	           ((toutcontrol == 5) || (toutcontrol == 7));

	if (assertIt != pitIntAsserted) {
		pitIntAsserted = assertIt;
		msgout (MSGC_INFO,MYSELF,MSG_NONE,"timer interrupt %s (tcr %02x tsr %02x)",
				assertIt ? "asserted" : "negated",tcr,pit_regs[TSR]);
		m68k_set_int_line (PIT_TINTR, assertIt ? ASSERT_LINE : CLEAR_LINE);
	}
}

int pit_irq_ack (int level) {
	int toutcontrol = (pit_regs[TCR] >> 5) & 0x07;

	if ((level == PIT_TINTR) && (pitIntAsserted)) {
		/* On this board only PC3/TOUT is wired, to INT6. TIACK is not connected,
		   so the 68230 can never supply a vector however its TOUT control is
		   programmed, and the timer is autovectored. Three things agree: the
		   boot PROM installs its handler at the level 6 autovector while it
		   programs the timer, the kernel installs one there too, and the vector
		   register only ever receives a register test pattern from the PROM.
		   toutcontrol is read only to keep the intent visible. */
		(void)toutcontrol;
		return M68K_INT_ACK_AUTOVECTOR;
	}
	return M68K_INT_ACK_SPURIOUS;
}

void pit_check_interrupt(void) {
	int tcr = pit_regs[TCR];
	int toutcontrol;

	if (tcr & 1) { 	/* is counter enabled ? */
		toutcontrol = (tcr >> 5) & 0x07; 
		if (counterValue == 0) {		/* check interrupt generation */
			if ((toutcontrol == 5) ||	/* 101 */
			    (toutcontrol == 7)) {	/* 111 */
				pit_regs[TSR] |= 0x01;      /* zero detect status */
				pit_update_irq();
			}
		}
	}
}



void pit_pulse_counter(void) {
	int tcr = pit_regs[TCR];
	
	if (tcr & 1) {	/* is counter enabled ? */
		if (counterValue == 0) {
			if (tcr & 0x10) counterValue = 0xffffff; else counterValue = counterPreloadValue;
		}
		counterValue--;
		if (counterValue == 0) pit_check_interrupt();
	}
}

/******************************************************************************
 * Commands
 ******************************************************************************/


void pit_showRegs(int numArgs, struct args_t *args) {
	int i;

	for (i=0;i<PIT_MAX_REGISTERS; i++)
		printf("%2d %s: %02x\n",i,pit_regNames[i],pit_regs[i]);
}

void pit_help (int numArgs, struct args_t *args);

struct cmds_t pitCmds[] =
{
	{ "registers",	pit_showRegs, 0,0,0,"show pit registers"},
	{ "?",			pit_help	, 0,0,0,"show this help"},
	{ "help",		pit_help	, 0,0,0,"show this help"},
    { "",  NULL, 0,0,0,""}
};

void pit_help (int numArgs, struct args_t *args) {
	showHelp ("pit help commands",pitCmds,0);
}


int pit_dbgCmd(int numArgs, struct args_t * args) {
	return findAndExecCommand (args[0].txt,pitCmds,numArgs-1,&args[1]);
}

int pit_save_state(FILE * f) {
    STATEWRITEVARS("pit");

    STATEWRITE(id,f);
    STATEWRITELEN(pit_regs,f);
    STATEWRITELEN(counterValue,f);
    STATEWRITELEN(counterPreloadValue,f);
    return 1;
}

int pit_load_state(FILE * f) {
    STATEREADVARS("pit");

    STATEREADID(f);
    STATEREADLEN(pit_regs,f);
    STATEREADLEN(counterValue,f);
    STATEREADLEN(counterPreloadValue,f);
    return 1;
}
