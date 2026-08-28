/***************************************************************************
 *  sim.c
 *
 *  Created: Nov, 22 2011
 *  Changed: Dec, 25 2020
 *  Armin Diehl <ad@ardiehl.de>
 ****************************************************************************
 * mai basic four system 2000 (eagle) emaulator main
 *
 * Nov 25 2020 AD: added nmi command
                   less verbose as default
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "sim.h"
#include "m68k.h"
#include <ctype.h>
#include "util.h"
#include <readline/readline.h>
#include <readline/history.h>
#include "memory.h"		/* raw ram and rom access */
#include "mmu.h"
#include "cmb.h"		/* cmb registers */
#include "fourway.h"	/* 4-way controller */
#include "wd.h"			/* winchester disk controller */
#include "cs.h"			/* streamer controller */
#include "nvram.h"		/* nvram on cmb */
#include "scc.h"		/* serial port controller on cmb */
#include "pit.h"		/* parallel interface, timer */
#include "fd.h"			/* floppy */
#include "load.h"		/* s-record loader */
#include "version.h"

#define MYSELF MSG_OTHER

/* Prototypes */
void exit_error(char* fmt, ...);


unsigned int cpu_read_byte(unsigned int address);
unsigned int cpu_read_word(unsigned int address);
unsigned int cpu_read_long(unsigned int address);
void cpu_write_byte(unsigned int address, unsigned int value);
void cpu_write_word(unsigned int address, unsigned int value);
void cpu_write_long(unsigned int address, unsigned int value);
unsigned int m68k_read_disassembler_8  (unsigned int address);
unsigned int m68k_read_disassembler_16 (unsigned int address);
unsigned int m68k_read_disassembler_32 (unsigned int address);

void cpu_pulse_reset(void);
void cpu_set_fc(unsigned int fc);

void dbgCmd_help (int numArgs, struct args_t *args);


int g_ctrlCpressed = 0;
int g_busErrorCount = 0;
INT32 g_breakOnBusError = 0;
int g_msgBreak = 0;
INT32 g_msgBreakEnabled = 0;
unsigned int g_currPC;

/* Write watchpoints. The breakpoints above trigger on the program counter,
 * which is no use when the question is "who writes this variable". These
 * trigger on a store to an address range and print the storing instruction.
 * Every write goes through sys_write_byte and sys_write_word, so DMA from the
 * disk and tape controllers is caught as well as CPU stores. */
#define MAXWATCHPOINTS 4
struct watchpoint_t {
    unsigned int addr;
    unsigned int len;
    unsigned int hits;
    int          brk;       /* stop the emulation as well as logging */
};
struct watchpoint_t watchpoints[MAXWATCHPOINTS];
int watchHit = 0;

void sys_watch_check (unsigned int address, unsigned int value, int size) {
    int i;
    char instruction[128];

    for (i = 0; i < MAXWATCHPOINTS; i++) {
        if (!watchpoints[i].len) continue;
        if ((address + size - 1 < watchpoints[i].addr) ||
            (address >= watchpoints[i].addr + watchpoints[i].len)) continue;
        watchpoints[i].hits++;
        m68k_disassemble(&instruction[0], g_currPC, M68K_CPU_TYPE_68010);
        printf("watch %d: write%d %0*x to %08x (PC:%08x %s)\n",
                i, size * 8, size * 2, value, address, g_currPC, instruction);
        fflush(stdout);
        if (watchpoints[i].brk) watchHit = 1;
    }
}

extern m68ki_cpu_core m68k_cpu;

void sim_pulse_bus_error (void) {
	if (g_breakOnBusError) g_busErrorCount++;
	/* printf("sim_pulse_bus_error\n"); */
	m68k_cpu.cpu_buserror_occurred = 1;  	 /* flag that bus error has occurred from cpu */
}

void ctrlChandler(int sig) {
	if (g_ctrlCpressed) exit(sig);
	g_ctrlCpressed++;
}

void setCtrlCPressed(void) {        /* for break from scc.c */
    g_ctrlCpressed++;
}

/* Exit with an error message.  Use printf syntax. */
void exit_error(char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");

	exit(EXIT_FAILURE);
}

/******************************************************************
 * Dispatcher for memory access
 *****************************************************************/

/* Read data from RAM, ROM, or a device */
unsigned int sys_read_byte(unsigned int address, int memFlags)
{
	unsigned int value;

	if (ADDR_IS_RAMSPACE(address) || ADDR_IS_ROM(address))
		value = mem_read_byte(address,memFlags);
	else
	if (ADDR_IS_CMB(address))
		value = cmb_read_byte(address);
	else
	if ((ADDR_IS_WD0(address)) || (ADDR_IS_WD1(address)))
		value = wd_read_byte(address,memFlags);
    else
    if (ADDR_IS_CS(address))
		value = cs_read_byte(address,memFlags);
	else
	if (ADDR_IS_NV(address))
		value = nv_read_byte(address);
	else
	if (ADDR_IS_SCC(address))
		value = scc_read_byte(address);
	else
	if (ADDR_IS_PIT(address))
		value = pit_read_byte(address,memFlags);
	else
	if (ADDR_IS_FD(address))
		value = fd_read_byte(address,memFlags);
	else
	if (ADDR_IS_MMU(address))
		value = mmu_read_byte(address);
	else
	if (ADDR_IS_FW(address))
		value = fw_read_byte(address,memFlags);
	else {
		/* msgout (MSGC_ERR,MSG_NONE,MSG_READB,"unknown memory area %08x",address); */
		BUSERROR(memFlags,address,MSG_READB);
		value = 0xff;
	}

	if (m68k_cpu.cpu_buserror_occurred) {
		m68k_cpu.cpu_buserror_address = address;
		m68k_cpu.cpu_buserror_on_write = 0;
		m68k_cpu.cpu_buserror_byte_transfer = 1;
	}

	return value;
}


unsigned int m68k_read_unaligned_32 (unsigned int address) {
	return
		sys_read_byte(address,MEM_DISABLEBUSERROR) << 24 |
		sys_read_byte(address+1,MEM_DISABLEBUSERROR) << 16 |
		sys_read_byte(address+2,MEM_DISABLEBUSERROR) << 8 |
		sys_read_byte(address+3,MEM_DISABLEBUSERROR);
}

unsigned int sys_read_word(unsigned int address, int memFlags)
{
	unsigned int value;

	if (ADDR_IS_RAMSPACE(address) || ADDR_IS_ROM(address))
		value = mem_read_word(address,memFlags);
	else
	if (ADDR_IS_CMB(address))
		value = cmb_read_word(address);
	else
	if ((ADDR_IS_WD0(address)) || (ADDR_IS_WD1(address)))
		value = wd_read_word(address,memFlags);
    else
    if (ADDR_IS_CS(address))
        value = cs_read_word(address,memFlags);
	else
	if (ADDR_IS_NV(address))
		value = nv_read_word(address);
	else
	if (ADDR_IS_SCC(address))
		value = scc_read_word(address);
	else
	if (ADDR_IS_PIT(address))
		value = pit_read_word(address,memFlags);
	else
	if (ADDR_IS_FD(address))
		value = fd_read_word(address,memFlags);
	else
	if (ADDR_IS_MMU(address))
		value = mmu_read_word(address);
	else
	if (ADDR_IS_FW(address))
		value = fw_read_word(address,memFlags);
	else {
		/*msgout (MSGC_ERR,MSG_NONE,MSG_READW,"unknown memory area %08x",address);*/
		BUSERROR(memFlags,address,MSG_READW);
		value = 0xffff;
	}
	if (m68k_cpu.cpu_buserror_occurred) {
		m68k_cpu.cpu_buserror_address = address;
		m68k_cpu.cpu_buserror_on_write = 0;
		m68k_cpu.cpu_buserror_byte_transfer = 0;
	}
	return value;
}

unsigned int sys_read_long (unsigned int address, int memFlags) {
    unsigned int data;

	data = sys_read_word(address,0) << 16;
	if (!(m68k_cpu.cpu_buserror_occurred))
		data |= sys_read_word(address+2,0);
	return data;
}


/* Write data to RAM or a device */
void sys_write_byte(unsigned int address, unsigned int value, int memFlags)
{
	sys_watch_check(address,value,1);
	if (ADDR_IS_RAMSPACE(address) || ADDR_IS_ROM(address))
			mem_write_byte(address,value,memFlags);
	else
	if (ADDR_IS_CMB(address))
		cmb_write_byte(address,value);
	else
	if ((ADDR_IS_WD0(address)) || (ADDR_IS_WD1(address)))
		wd_write_byte(address,value, memFlags);
    else
    if (ADDR_IS_CS(address))
    	cs_write_byte(address,value, memFlags);
	else
	if (ADDR_IS_NV(address))
		nv_write_byte(address,value);
	else
	if (ADDR_IS_SCC(address))
		scc_write_byte(address,value);
	else
	if (ADDR_IS_PIT(address))
		pit_write_byte(address,value,memFlags);
	else
	if (ADDR_IS_FD(address))
		fd_write_byte(address,value,memFlags);
	else
	if (ADDR_IS_MMU(address))
		mmu_write_byte(address,value);
	else
	if (ADDR_IS_FW(address))
		fw_write_byte(address,value,memFlags);
	else
		/*msgout (MSGC_ERR,MSG_NONE,MSG_WRITEB,"%02x to unknown memory %08x",value,address);*/
		BUSERROR(memFlags,address,MSG_WRITEB);

	if (m68k_cpu.cpu_buserror_occurred) {
		m68k_cpu.cpu_buserror_address = address;
		m68k_cpu.cpu_buserror_on_write = 1;
		m68k_cpu.cpu_buserror_writeval = value;
		m68k_cpu.cpu_buserror_byte_transfer = 1;
	}
}

void sys_write_word(unsigned int address, unsigned int value, int memFlags)
{
	sys_watch_check(address,value,2);
	if (ADDR_IS_RAMSPACE(address) || ADDR_IS_ROM(address))
		mem_write_word(address,value,memFlags);
	else
	if (ADDR_IS_CMB(address))
		cmb_write_word(address,value);
	else
	if ((ADDR_IS_WD0(address)) || (ADDR_IS_WD1(address)))
		wd_write_word(address,value,memFlags);
    else
    if (ADDR_IS_CS(address))
        cs_write_word(address,value,memFlags);
	else
	if (ADDR_IS_NV(address))
		nv_write_word(address,value);
	else
	if (ADDR_IS_SCC(address))
		scc_write_word(address,value);
	else
	if (ADDR_IS_PIT(address))
		pit_write_word(address,value,memFlags);
	else
	if (ADDR_IS_FD(address))
		fd_write_word(address,value,memFlags);
	else
	if (ADDR_IS_MMU(address))
		mmu_write_word(address,value);
	else
	if (ADDR_IS_FW(address))
		fw_write_word(address,value,memFlags);
	else
		/*msgout (MSGC_ERR,MSG_NONE,MSG_WRITEW,"%04x to unknown memory %08x",value,address);*/
		BUSERROR(memFlags,address,MSG_WRITEW);

	if (m68k_cpu.cpu_buserror_occurred) {
		m68k_cpu.cpu_buserror_address = address;
		m68k_cpu.cpu_buserror_on_write = 1;
		m68k_cpu.cpu_buserror_writeval = value;
		m68k_cpu.cpu_buserror_byte_transfer = 0;
	}
}


void sys_write_long(unsigned int address, unsigned int value, int memFlags) {
    cpu_write_word(address, (value >> 16) & 0xffff);
	if (!(m68k_cpu.cpu_buserror_occurred))
		cpu_write_word(address+2, value & 0xffff);
}

/*******************************************************************
 * memory routines called from musashi
 *******************************************************************/

/* The MMU sits between the 68010 and everything else, but only in user mode.
 * In supervisor mode the CPU addresses physical memory directly and uses the
 * 8XXXXX and AXXXXX windows to load the segment descriptors, see mmu.c.
 * A management violation is reported to the CPU as a bus error, which is what
 * the real board does with MMERR-, and which BOSS/IX prints as
 * "68010 memory management trap".
 */
static int cpu_in_user_mode(void) {
	return (m68k_get_reg(NULL,M68K_REG_SR) & 0x2000) ? 0 : 1;
}

static int cpu_xlate(unsigned int logical, int isWrite, unsigned int * phys) {
	if (!cpu_in_user_mode() || !mmu_is_enabled()) { *phys = logical; return 1; }
	if (mmu_translate(logical,isWrite,phys)) return 1;
	return 0;
}

unsigned int cpu_read_byte(unsigned int address) {
	unsigned int phys;
	if (!cpu_xlate(address,0,&phys)) { BUSERROR(0,address,MSG_READB); return 0xff; }
	return sys_read_byte(phys,0);
}

unsigned int cpu_read_word(unsigned int address) {
	unsigned int phys;
	if (!cpu_xlate(address,0,&phys)) { BUSERROR(0,address,MSG_READW); return 0xffff; }
	return sys_read_word(phys,0);
}

unsigned int cpu_read_long(unsigned int address)
{
	unsigned int data;

	data = cpu_read_word(address) << 16;
	if (!(m68k_cpu.cpu_buserror_occurred))
		data |= cpu_read_word(address+2);
	return data;
}

void cpu_write_byte(unsigned int address, unsigned int value) {
	unsigned int phys;
	if (!cpu_xlate(address,1,&phys)) { BUSERROR(0,address,MSG_WRITEB); return; }
	sys_write_byte(phys,value,0);
}

void cpu_write_word(unsigned int address, unsigned int value) {
	unsigned int phys;
	if (!cpu_xlate(address,1,&phys)) { BUSERROR(0,address,MSG_WRITEW); return; }
	sys_write_word(phys,value,0);
}

void cpu_write_long(unsigned int address, unsigned int value) {
	cpu_write_word(address, (value >> 16) & 0xffff);
	if (!(m68k_cpu.cpu_buserror_occurred))
		cpu_write_word(address+2, value & 0xffff);
}


unsigned int m68k_read_disassembler_8  (unsigned int address) {
	return sys_read_byte (address,MEM_DISABLEBUSERROR);
}


unsigned int m68k_read_disassembler_16 (unsigned int address) {
	return sys_read_word (address,MEM_DISABLEBUSERROR);
}


unsigned int m68k_read_disassembler_32 (unsigned int address) {
	unsigned int data;

	data = sys_read_word(address,MEM_DISABLEBUSERROR) << 16;
	data |= sys_read_word(address+2,MEM_DISABLEBUSERROR);
	return data;
}


/*******************************************************************
 * callbacks from musashi
 *******************************************************************/

int sys_int_ack (device_t *device, int int_level){
    unsigned int vector;
	/* TODO: int ack for other vector devices (wd,fw,ts) */
    vector = cs_irq_ack(int_level);     /* has cs raised the int ? */
    if (vector == M68K_INT_ACK_SPURIOUS) vector = wd_irq_ack(int_level);
    if (vector == M68K_INT_ACK_SPURIOUS) vector = fw_irq_ack(int_level);
    if (vector == M68K_INT_ACK_SPURIOUS) vector = pit_irq_ack(int_level);
    if (vector != M68K_INT_ACK_SPURIOUS) {
        /*printf("sys_int_ack: returning vector %02x\n",vector);*/
        return vector;
    }

	return M68K_INT_ACK_AUTOVECTOR;
}

/* Ring of recently executed program counters. The instruction hook already
 * receives the PC of every instruction about to run, so recording it is nearly
 * free and it answers the one question a watchpoint cannot: how did control
 * get here. Dump it with the "history" debugger command after a breakpoint. */
#define PCHISTORY_SIZE 256
unsigned int pcHistory[PCHISTORY_SIZE];
unsigned char pcHistSuper[PCHISTORY_SIZE];
unsigned int pcHistoryIdx = 0;
unsigned int pcHistoryCount = 0;

/* System call tracing.
 *
 * BOSS/IX reaches the kernel the usual 68k way, with a TRAP instruction, so the
 * question "what is the first user process actually asking for" is answered by
 * watching user mode instructions for opcode 4E4x. Only user mode is examined,
 * which is a small fraction of the instruction stream, and the program counter
 * is translated through a read only copy of the MMU arithmetic so that tracing
 * does not disturb the segment status bits or the translation counters. */
int trapTrace = 0;

static int trapPending = 0;

/* Read a user mode long through the same translation the CPU would use, or
   report failure rather than disturbing anything. */
static int sys_peek_user_long (unsigned int logical, unsigned int * out) {
    unsigned int phys;

    if (mmu_is_enabled()) {
        if (!mmu_peek_translate(logical,&phys)) return 0;
    } else phys = logical;
    *out = (sys_read_word(phys,MEM_DISABLEBUSERROR) << 16) |
            sys_read_word(phys+2,MEM_DISABLEBUSERROR);
    return 1;
}

/* Print a NUL terminated user string, for the calls that take a path. */
static void sys_peek_user_string (unsigned int logical, int max) {
    unsigned int w;
    unsigned char c;
    int k;

    printf("\"");
    for (k = 0; k < max; k++) {
        if (!sys_peek_user_long((logical + k) & ~3u,&w)) { printf("<untranslatable>"); break; }
        c = (w >> (8 * (3 - ((logical + k) & 3)))) & 0xff;
        if (!c) break;
        if ((c >= 32) && (c < 127)) putchar(c); else printf("<%02x>",c);
    }
    printf("\"");
}

static void sys_trace_traps (unsigned int pc) {
    unsigned int phys;
    UINT16 op;
    unsigned int d0, d1, d2, a0, a1, a7, i, arg;

    if (m68k_get_reg(NULL,M68K_REG_SR) & 0x2000) return;    /* supervisor */

    /* the first user instruction after a trap carries its result */
    if (trapPending) {
        trapPending = 0;
        printf("      -> returned D0=%08x D1=%08x, back at %06x\n",
                m68k_get_reg(NULL,M68K_REG_D0),
                m68k_get_reg(NULL,M68K_REG_D1), pc);
        fflush(stdout);
    }

    if (mmu_is_enabled()) {
        if (!mmu_peek_translate(pc,&phys)) return;
    } else phys = pc;

    op = sys_read_word(phys,MEM_DISABLEBUSERROR) & 0xffff;
    if ((op & 0xfff0) != 0x4e40) return;                    /* not a TRAP */

    d0 = m68k_get_reg(NULL,M68K_REG_D0);
    d1 = m68k_get_reg(NULL,M68K_REG_D1);
    d2 = m68k_get_reg(NULL,M68K_REG_D2);
    a0 = m68k_get_reg(NULL,M68K_REG_A0);
    a1 = m68k_get_reg(NULL,M68K_REG_A1);
    a7 = m68k_get_reg(NULL,M68K_REG_A7);
    printf("trap #%u call %02x at %06x  D0=%08x D1=%08x D2=%08x A1=%08x sp=%06x  stack:",
            op & 0x0f, a0 & 0xff, pc, d0, d1, d2, a1, a7);
    for (i = 0; i < 5; i++) {
        if (sys_peek_user_long(a7 + i*4,&arg)) printf(" %08x",arg);
        else                                   printf(" --------");
    }
    printf("\n");
    /* A write style call: D1 points at the buffer and D2 holds the length,
       which is how the call pid 1 repeats during boot reads. Dumping it turns
       an opaque syscall number into the actual message. */
    /* calls that plausibly carry a path in D1 */
    switch (a0 & 0xff) {
        case 0x05: case 0x0a: case 0x0d: case 0x27: case 0x47: case 0x9b:
            printf("      arg D1 as string: ");
            sys_peek_user_string(d1,72);
            printf("\n");
            printf("      arg D0 as string: ");
            sys_peek_user_string(d0,72);
            printf("\n");
            break;
    }
    if (((a0 & 0xff) == 0x0e) && (d2 > 0) && (d2 < 512)) {
        unsigned int k, w;
        unsigned char c;
        printf("      text: \"");
        for (k = 0; k < d2; k++) {
            if (!sys_peek_user_long((d1 + k) & ~3u,&w)) { printf("<untranslatable>"); break; }
            c = (w >> (8 * (3 - ((d1 + k) & 3)))) & 0xff;
            if ((c >= 32) && (c < 127)) putchar(c);
            else if (c == 10) printf("\\n");
            else if (c == 13) printf("\\r");
            else printf("<%02x>",c);
        }
        printf("\"\n");
    }
    fflush(stdout);
    trapPending = 1;
}

unsigned char timerInstrCount = 0;
unsigned char wdInstrCount = 0;
int fdInstrCount = 0;
unsigned int m68k_instruction_count = 0;

void sys_instr_hook (device_t * device, unsigned int pc) {
    m68k_instruction_count++;  // used by fd

	pcHistory[pcHistoryIdx] = pc;
	pcHistSuper[pcHistoryIdx] = (m68k_get_reg(NULL,M68K_REG_SR) & 0x2000) ? 1 : 0;
	pcHistoryIdx = (pcHistoryIdx + 1) & (PCHISTORY_SIZE - 1);
	if (pcHistoryCount < PCHISTORY_SIZE) pcHistoryCount++;

	if (trapTrace) sys_trace_traps(pc);
}

/* Device time. This used to live in the instruction hook, which stops being
 * called the moment the CPU executes STOP, because a stopped CPU executes no
 * instructions. BOSS/IX idles in stop #$2000 waiting for the timer tick, so
 * the timer only advanced while the machine was busy and froze the moment it
 * went idle: date stood still, shutdown's countdown hung at its first line,
 * and every timeout in the kernel waited forever. The run loops call this on
 * every iteration instead; m68k_execute returns immediately while stopped, so
 * the loop keeps spinning and time keeps passing, which is what a crystal
 * driven 68230 does on the real board. */
/* Counting instructions was wrong twice over. While the CPU spins in stop
 * #$2000 there are no instructions to count, so the loop pulsed the 68230 at
 * whatever rate the host could spin: measured at 6.31 times real time, and at
 * a hundred per cent of a core, which on a small virtual machine leaves
 * nothing for the console and makes typing lag. The 68230 on the real board is
 * driven by a crystal, so drive it from the host clock instead. */
/* One 68230 count, in nanoseconds. Calibrated by running the machine idle for
 * a minute and comparing its date against the host clock; see NOTES.md. */
#ifndef PIT_PERIOD_NS
#define PIT_PERIOD_NS	500
#endif
#define PIT_RESYNC_NS	20000000L	/* never make up more than 20 ms in one go */

static long long host_ns (void) {
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static long long pitDue = 0;

void sys_device_tick (void) {
	long long now;
	int idle = m68k_is_stopped();

	if (idle) {
		/* Nothing to execute until an interrupt arrives, so give the core
		 * back to the host rather than spinning on it. Keep the pause short:
		 * the disk controller is serviced from this same loop, and the kernel
		 * waits for it stopped, so a long pause here is a slow boot. */
		usleep (100);
	}

	/* Reading the host clock costs more than emulating the instruction does,
	 * so only look while idle, where we just slept anyway, or every so many
	 * instructions while running. */
	timerInstrCount++;
	if (idle || timerInstrCount >= 64) {
		timerInstrCount = 0;
		now = host_ns();
		if (!pitDue || now - pitDue > PIT_RESYNC_NS) pitDue = now;
		while (pitDue <= now) {
			pit_pulse_counter();
			pitDue += PIT_PERIOD_NS;
		}
	}
	/* One idle pass stands for the instructions the CPU would have executed in
	 * that time, so the disk keeps its cadence instead of stalling. */
    wdInstrCount += idle ? 20 : 1;
	if (wdInstrCount >= 20) {
		wd_processContinue();
        cs_processContinue();
		wdInstrCount = 0;
	}
	if (fdInstrCount) {
		int step = idle ? 20 : 1;
		fdInstrCount -= (fdInstrCount > step) ? step : fdInstrCount;
		if (fdInstrCount == 0) fd_processContinue();
	}
}

void fd_setContinueCounter (int countDown) {
	fdInstrCount = countDown;
}

/*******************************************************************/



int numMsgs = 0;
UINT32 msgClassFlags[MSGC_MAX];

int messageIsEnabled (int msgClass,int msgSource) {
  return (msgClassFlags[msgClass] & (1<<msgSource));
}

INT32 g_showDuplicateMessages = 1;
char g_lastMessage[512];

int  msgout(unsigned int msgClass,
            unsigned int msgSource,
            unsigned int routine,
            char * msg, ...) {
	va_list argptr;
	char classSrc[64];   /* "NOT YET IMPLEMENTED " + source + routine is 32 */
	char message[512];
    char msgBreak[20];
    int res = 0;
    char instruction[255];

	msgBreak[0]=0;
    if (msgClass & MSGC_BREAK) {
        msgClass &= ~MSGC_BREAK;
        if (g_msgBreakEnabled) {
            g_msgBreak++;
            strcpy(msgBreak,"+break ");
        }
    }

	if ((msgClass != MSGC_FATAL) && (msgBreak[0] == 0) && (!(msgClassFlags[msgClass] & (1<<msgSource)))) return 0;

	va_start(argptr,msg);
	vsnprintf(message,sizeof(message),msg,argptr);
	va_end(argptr);

	classSrc[0]=0;
	switch (msgClass) {
		case (MSGC_ERR)  	: { strcat(classSrc,"ERROR "); break; }
		case (MSGC_NOTIMP)  : { strcat(classSrc,"NOT YET IMPLEMENTED "); break; }
		case (MSGC_WARN) 	: { strcat(classSrc,"WARNING "); break; }
		case (MSGC_FATAL)	: { strcat(classSrc,"FATAL "); break; }
	}
    switch (msgSource) {
		case (MSG_OTHER): { strcat(classSrc,"unknown: "); break; }
		case (MSG_CPU)	: { strcat(classSrc,"cpu: "); break; }
		case (MSG_CMB)	: { strcat(classSrc,"cmb: "); break; }
		case (MSG_NV)	: { strcat(classSrc,"nvram: "); break; }
		case (MSG_FW)	: { strcat(classSrc,"fw: "); break; }
		case (MSG_WD)	: { strcat(classSrc,"wd: "); break; }
		case (MSG_SCC)	: { strcat(classSrc,"scc: "); break; }
		case (MSG_CS)	: { strcat(classSrc,"cs: "); break; }
		case (MSG_PIT)	: { strcat(classSrc,"pit: "); break; }
		case (MSG_MEM)	: { strcat(classSrc,"memory: "); break; }
		case (MSG_FD)	: { strcat(classSrc,"fd: "); break; }
	}
	switch (routine) {
		case (MSG_READB)	: { strcat(classSrc,"read8 "); break; }
		case (MSG_READW)	: { strcat(classSrc,"read16 "); break; }
		case (MSG_READL)	: { strcat(classSrc,"read32 "); break; }
		case (MSG_WRITEB)	: { strcat(classSrc,"write8 "); break; }
		case (MSG_WRITEW)	: { strcat(classSrc,"write16 "); break; }
		case (MSG_WRITEL)	: { strcat(classSrc,"write32 "); break; }
		case (MSG_SAVE)		: { strcat(classSrc,"save "); break; }
		case (MSG_LOAD)		: { strcat(classSrc,"load "); break; }
	}
    if ((g_showDuplicateMessages) || (strcmp(message,g_lastMessage) != 0)) {
        m68k_disassemble(&instruction[0], g_currPC, M68K_CPU_TYPE_68010);
	    printf("%s%s%s (PC:%08x %s)\n",classSrc,msgBreak,message,g_currPC,instruction);
        res = 1;
    }
	numMsgs++;
    strcpy(g_lastMessage,message);

	if (msgClass == MSGC_FATAL) {
		kb_normal();
		exit(EXIT_FAILURE);
	}
    return res;
}


void msgoutn(unsigned int msgClass,
             unsigned int msgSource,
             unsigned int routine,
             char * msg, ...) {
    char message[512];
    va_list argptr;

    va_start(argptr,msg);
    vsnprintf(message,sizeof(message),msg,argptr);
    va_end(argptr);
    if (msgout(msgClass,msgSource,routine,message)) printf("\n");
}



void fatalerror(char * msg, ...) {
	char message[512];
	va_list argptr;

	va_start(argptr,msg);
	vsnprintf(message,sizeof(message),msg,argptr);
	va_end(argptr);
	msgout (MSGC_FATAL,MSG_OTHER,MSG_NONE,"%s",message);
}


void pollBoardStatus(void) {
	scc_pollStatus ();
}

/* ======================================================================== */
/* =================== eagle like debugger functions ====================== */
/* ======================================================================== */


int showInstruction(unsigned int pc, char * comment)
{
	char buf[255];
	char hx[5];
	char instHex[255];
	unsigned int instrLen;
	int i;

	instrLen = m68k_disassemble(&buf[0], pc, M68K_CPU_TYPE_68010) & DASMFLAG_LENGTHMASK;
	instHex[0]=0;
	for (i=0;i<instrLen;i++) {
		sprintf(hx,"%02x ",cpu_read_byte(pc+i));
		strcat(instHex,hx);
	}
	printf("%08x %-24s  %-35s %s\n",pc,instHex,buf,comment);
	return instrLen;
}


void showBusError (char *txt) {
	if (m68k_cpu.cpu_buserror_on_write)
		printf("%s bus error @%08x write to %08x\n",txt,m68k_cpu.cpu_buserror_instraddr,m68k_cpu.cpu_buserror_address);
	else if (m68k_cpu.cpu_buserror_instrfetch)
		printf("%s bus error @%08x while fetching instruction from %08x\n",txt,m68k_cpu.cpu_buserror_instraddr,m68k_cpu.cpu_buserror_address);
	else
		printf("%s bus error @%08x read from %08x\n",txt,m68k_cpu.cpu_buserror_instraddr,m68k_cpu.cpu_buserror_address);
}

void flagsTxt (unsigned int flags, char * txt) {
	sprintf(txt,"%c%c%c%c%c%c",
	        (flags & 0x20000000) ? 'S' : 's',
	        (flags & 0x10) ? 'X' : 'x',
	        (flags & 0x08) ? 'N' : 'n',
	        (flags & 0x04) ? 'Z' : 'z',
	        (flags & 0x02) ? 'V' : 'v',
	        (flags & 0x01) ? 'C' : 'c');
}


int numArgs;

struct args_t args [MAXNUMARGS];


void invalidArgs(void) {
	printf("\ninvalid arguments\n");
}

char inputHexval (unsigned int * value, const char * exitChars) {
	char s[10];
	unsigned int v;
	char c = inputString (s, 8, exitChars);
	if (strlen(s) > 0) {
		if (!(xtoui(s, &v))) {
			printf("\ninvalid hex value");
			return KEY_ESC;
		}
		*value = v;
	}
	return c;
}


void setOnOff (INT32 * target, char * txt) {
	if (strcasecmp(txt,"on")==0) { *target=1; return; }
	if (strcasecmp(txt,"off")==0) { *target=0; return; }
	if (txt[1] == 0) {
		switch (txt[0]) {
			case '0': { *target=0; return; }
			case '1': { *target=1; return; }
			case '-': { *target=0; return; }
			case '+': { *target=1; return; }
		}
	}
	printf(" 0|1, +|- or on|off expected\n");
}

void dbgCmd_bus(int numArgs, struct args_t *args) {
	if (numArgs < 1) {
		printf("Bus-error break is %s\n",g_breakOnBusError ? "on" : "off");
	} else setOnOff(&g_breakOnBusError,args[0].txt);
}


void dbgCmd_mbrk(int numArgs, struct args_t *args) {
	if (numArgs < 1) {
		printf("Message break is %s\n",g_msgBreakEnabled ? "on" : "off");
	} else setOnOff(&g_msgBreakEnabled,args[0].txt);
}

void dbgCmd_dup(int numArgs, struct args_t *args) {
	if (numArgs < 1) {
		printf("Duplicate Messages %s\n",g_breakOnBusError ? "on" : "off");
	} else setOnOff(&g_showDuplicateMessages,args[0].txt);
}



/* msg {source|all} {-|+|{+|-}warn | {+|-}err | {+|-}info} */

char msgClasses[MSGC_MAX][10] = {"err","notimp","warn","info","fatal","func"};
char msgSources[][10] = {"other","cpu","cmb","nv","fw","wd","scc","cs","pit","mem","fd",""};

void listMsg(int msgSource) {
	int i=0;
	int j;
	char c;
	while (msgSources[i][0] != 0) {
		printf("%s: ",msgSources[i]);
		for (j=0;j<MSGC_MAX;j++) {
			if (msgClassFlags[j] & 1<<i) c = '+'; else c = '-';
			printf("%c%s ",c,msgClasses[j]);
		}
		printf("\n");
		i++;
	}
    /* for (j=0;j<MSGC_MAX;j++)
	    printf("%08x ",msgClassFlags[j]);
    printf("\n"); */
}

void msgSet(int classNum, int msgSource, int onOff) {
	unsigned int bitmask;
	if (msgSource == -1) {
		if (onOff) msgClassFlags[classNum] = 0xffffffff; else msgClassFlags[classNum] = 0;
		return;
	}
	bitmask = 1 << msgSource;
	if (onOff) {
		msgClassFlags[classNum] |= bitmask;
	} else {
		msgClassFlags[classNum] &= ~bitmask;
	}
}

void dbgCmd_msg(int numArgs, struct args_t *args) {
	int i,j;
	int onOff;
	int msgSource = -1;
	int currClassNum = -1;

	if (numArgs < 1) { listMsg(-1); return; }
	i=0;
	while ((msgSources[i][0] != 0) && (msgSource == -1)) {
		if (strcmp(args[0].txt,msgSources[i]) == 0) msgSource = i;
		i++;
	}
	if (msgSource == -1) {
		if (strcmp(args[0].txt,"all") != 0) {
			printf("msg: message source or all expected, valid sources are:\n");
			i=0;
			while (msgSources[i][0] != 0) { printf("%s ",msgSources[i]); i++; }
			printf("\n"); return;
		}
	}
	for (i=1;i<numArgs;i++) {
		switch (args[i].txt[0]) {
			case '-' : { onOff = 0; break; }
			case '+' : { onOff = 1; break; }
			default  : { printf ("+ or - expected as first char of arg %d (%s)\n",i,args[i].txt);
						 return;
			}
		}
		if (args[i].txt[1] != 0) {
			for (j=0;j<MSGC_MAX;j++) {
				if (strcmp(&args[i].txt[1],msgClasses[j]) == 0) currClassNum = j;
			}
			if (currClassNum == -1) {
				printf("msg: message class expected, valid classes are:\n");
				i=0;
			for (j=0;j<MSGC_MAX;j++) {
				printf("%s ",msgClasses[j]);
			}
			printf("\n"); return;
			}
		}
		/* switch on/off */
		if (currClassNum == -1) {
			for (i=0;i<MSGC_MAX;i++) msgSet(i,msgSource,onOff);
		} else
			msgSet(currClassNum,msgSource,onOff);
	}

}


void dbgCmd_pc(int numArgs, struct args_t *args) {
	unsigned int regVal;
	char c;

	if (numArgs > 0) {
		m68k_set_reg(M68K_REG_PC,args[0].value);
		printf("PC %08x\n",args[0].value);
	} else {
		regVal = m68k_get_reg(NULL,M68K_REG_PC);
		printf("PC %08x: ",regVal);
		c = inputHexval (&regVal,"\n ");
		if (c != KEY_ESC) m68k_set_reg(M68K_REG_PC,regVal);
		printf("\n");
	}
}



void dbgCmd_andn(char ad, int regBase, int numArgs, struct args_t *args) {
int regNum;
	unsigned int regVal;
	char c;

	ad = toupper(ad);
	if (numArgs < 1) { numArgs++; strcpy(args[0].txt," 0"); args[0].txt[0] = ad; }
	if ((strlen(args[0].txt) != 2) | (toupper(args[0].txt[0]) != ad)) {
		invalidArgs(); return;
	}
	regNum = args[0].txt[1]-'0';
	if ((regNum < 0) | (regNum > 7)) regNum = 0;
	do {
	  regVal = m68k_get_reg(NULL,regBase+regNum);
	  printf("%c%d %08x: ",ad,regNum,regVal);
	  c = inputHexval (&regVal,"\n /.");
	  if (c != KEY_ESC) m68k_set_reg(regBase+regNum,regVal);
	  printf("\n");
	  switch (c) {
		 case ' ': { regNum++; if (regNum > 7) { regNum = 0; } break; }
		 case '/': { if (regNum) regNum--; else regNum = 7; break; }
	  }
	} while ((c == '/') | (c == '.') | (c == ' '));
}

void dbgCmd_an (int numArgs, struct args_t *args)
{
	dbgCmd_andn('A',M68K_REG_A0,numArgs,args);
}


void dbgCmd_dn (int numArgs, struct args_t *args)
{
	dbgCmd_andn('D',M68K_REG_D0,numArgs,args);
}

void dbgCmd_dbdwdw (int byteSize, int ascii, int numArgs, struct args_t *args) {
	unsigned int addr;
	unsigned int value,valueOrg;
	char format[30];
	char buf[2];
	char c;
    int i;

	if ((numArgs < 1) | (!(args[0].isValue))) {
		invalidArgs(); return;
	}
	addr = args[0].value;

    switch (byteSize) {
		case 1: { if (ascii) strcpy(format,"  %08x %c"); else strcpy(format,"  %08x %02x");
			  break;
			}
		case 2: { strcpy(format,"  %08x %04x"); addr &= ~1; break; }
		case 4: { strcpy(format,"  %08x %08x"); addr &= ~2; break; }
	}

    if (numArgs > 1) {      /* dump only */
        if (args[1].isValue) {
            strcat(format,"\n");
            for (i=0;i<args[1].value;i++) {
                value = 0;	/* to avoid warning */
	            switch (byteSize) {
		            case 1: { value = cpu_read_byte(addr); break; }
		            case 2: { value = cpu_read_word(addr); break; }
		            case 4: { value = cpu_read_long(addr); break; }
	            }
                if (ascii) {
		            c = (char)value;
		            if ((c < ' ') | (c >= 0x07f)) c = '.';
		            printf(format,addr,c);
	            } else
		            printf(format,addr,value);
                addr+=byteSize;
            }
        } else {
            printf("numeric value required as 2nd parameter\n");
        }
        return;
    }

    strcat (format," : ");

    do {
	  value = 0;	/* to avoid warning */
	  switch (byteSize) {
		  case 1: { value = cpu_read_byte(addr); break; }
		  case 2: { value = cpu_read_word(addr); break; }
		  case 4: { value = cpu_read_long(addr); break; }
	  }
	  valueOrg = value;
	  if (ascii) {
		c = (char)value;
		if ((c < ' ') | (c >= 0x07f)) c = '.';
		printf(format,addr,c);
		c = inputString (buf, 1, "\n /.");
		if ((c != KEY_ESC) & (buf[0])) value = buf[0];
	  } else {
		  printf(format,addr,value);
	      c = inputHexval (&value,"\n /.");
	  }
	  if ((c != KEY_ESC) && (value != valueOrg)) {
		  switch (byteSize) {
  		    case 1: { cpu_write_byte(addr,value); break; }
		    case 2: { cpu_write_word(addr,value); break; }
		    case 4: { cpu_write_long(addr,value); break; }
	      }
	  }
	  printf("\n");
	  switch (c) {
		 case ' ': { addr+=byteSize; break; }
		 case '/': { addr-=byteSize; break; }
	  }
	} while ((c == '/') | (c == '.') | (c == ' '));
}


void dbgCmd_db (int numArgs, struct args_t *args) {
	dbgCmd_dbdwdw (1,0,numArgs,args);
}

void dbgCmd_dc (int numArgs, struct args_t *args) {
	dbgCmd_dbdwdw (1,1,numArgs,args);
}

void dbgCmd_dw (int numArgs, struct args_t *args) {
	dbgCmd_dbdwdw (2,0,numArgs,args);
}

void dbgCmd_dl (int numArgs, struct args_t *args) {
	dbgCmd_dbdwdw (4,0,numArgs,args);
}


void dbgCmd_regs (int numArgs, struct args_t *args) {
	char flags[30];
	unsigned int sr = m68k_get_reg(NULL,M68K_REG_SR);

	flagsTxt (sr,flags);

	printf("A0: %08x A1:%08x A2:%08x A3:%08x A4:%08x A5:%08x A6:%08x A7:%08x\n" \
	       "D0: %08x D1:%08x D2:%08x D3:%08x D4:%08x D5:%08x D6:%08x D7:%08x\n" \
	       "VBR:%08x PC:%08x SR:%08x %s\n",
	       m68k_get_reg(NULL,M68K_REG_A0),m68k_get_reg(NULL,M68K_REG_A1),
	       m68k_get_reg(NULL,M68K_REG_A2),m68k_get_reg(NULL,M68K_REG_A3),
	       m68k_get_reg(NULL,M68K_REG_A4),m68k_get_reg(NULL,M68K_REG_A5),
	       m68k_get_reg(NULL,M68K_REG_A6),m68k_get_reg(NULL,M68K_REG_A7),
	       m68k_get_reg(NULL,M68K_REG_D0),m68k_get_reg(NULL,M68K_REG_D1),
	       m68k_get_reg(NULL,M68K_REG_D2),m68k_get_reg(NULL,M68K_REG_D3),
	       m68k_get_reg(NULL,M68K_REG_D4),m68k_get_reg(NULL,M68K_REG_D5),
	       m68k_get_reg(NULL,M68K_REG_D6),m68k_get_reg(NULL,M68K_REG_D7),
	       m68k_get_reg(NULL,M68K_REG_VBR),
	       m68k_get_reg(NULL,M68K_REG_PC),sr,flags);
}



void dbgCmd_doDump (unsigned int addr, unsigned int endAddr, int lineLen) {
	unsigned int data;
	int asciiLen = 0;
	int hexLen;
	char hexData[100];
	char ascii[17];

	printf("\n");
	hexData[0]=0; sprintf(hexData,"%08x: ",addr);
	hexLen = strlen(hexData);
	while (addr <= endAddr) {
		data = cpu_read_byte(addr);
		if ((data < ' ') | (data > 0x7e)) ascii[asciiLen++] = '.'; else ascii[asciiLen++] = data;
		ascii[asciiLen] = 0;
		hexData[hexLen++] = hexNibble(data >> 4);
		hexData[hexLen++] = hexNibble(data);
		hexData[hexLen++] = ' ';
		hexData[hexLen] = 0;
		if (asciiLen == 8) { hexData[hexLen++] = ' '; hexData[hexLen] = 0; }
		addr++;
		if (asciiLen == lineLen) {
			printf("%-59s  %s\n",hexData,ascii);
			asciiLen = 0;
			hexData[0]=0; sprintf(hexData,"%08x: ",addr);
			hexLen = strlen(hexData);
		}
	}
	if (asciiLen > 0) {
		printf("%-59s  %s\n",hexData,ascii);
	}
}



void dbgCmd_type (int numArgs, struct args_t *args) {
	unsigned int addr = args[0].value;
	unsigned int endAddr = args[1].value;
	int lineLen;
	if (numArgs > 2) lineLen = args[2].value; else lineLen = 16;
	if (numArgs < 2) endAddr = addr + 64;

	dbgCmd_doDump (addr,endAddr,lineLen);
}


void dbgCmd_dump (int numArgs, struct args_t *args) {
	unsigned int addr = args[0].value;
	unsigned int endAddr = addr + args[1].value;
	int lineLen;
	if (numArgs > 2) lineLen = args[2].value; else lineLen = 16;

	dbgCmd_doDump (addr,endAddr-1,lineLen);
}


void dbgCmd_disass (int numArgs, struct args_t *args) {
	unsigned int addr = args[0].value;
	unsigned int maxCount = args[1].value;
	int i = 0;
	if (maxCount < 1) maxCount = 16;

	do {
		addr += showInstruction(addr,"");
		i++;
	} while (i < maxCount);
}


char  regs[][4] = {
	"D0","D1","D2","D3","D4","D5","D6","D7",
	"A0","A1","A2","A3","A4","A5","A6","A7",
	"PC","SR","SP","USP","ISP","MSP","SFC","DFC","VBR",""};



#define NUMREGS 25


void dbgCmd_rese (int numArgs, struct args_t *args) {
	mem_pulse_reset();
	m68k_pulse_reset();
	cmb_pulse_reset();
	scc_pulse_reset();
	wd_pulse_reset();
	mmu_pulse_reset();
    cs_pulse_reset();
	nv_pulse_reset();
	fw_pulse_reset();
	fd_pulse_reset();
	pit_pulse_reset();
	m68k_set_instr_callback(sys_instr_hook);	/* agghh: cleaed by reset */
}




#define MAXBREAKPOINTS 4

struct breakpoint_t
  { unsigned int addr;
	int count;
	int currCount;
  };

struct breakpoint_t breakpoints [MAXBREAKPOINTS] = {
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 }
};

unsigned int tempBreakpoint = 0xffffffff;  /* for step over */

int breakpointReached (unsigned int addr) {
	int i;

	if (addr == tempBreakpoint) {
        tempBreakpoint = 0xffffffff;
        /*printf ("Temp break\n");*/
        return -1; /* used for step over */
    }

    if (g_msgBreak) {
        g_msgBreak--;
        return -3;
    }

	/* break on bus error if enabled */
	if ((g_breakOnBusError) && (g_busErrorCount)) return -2;

	for (i=0;i<MAXBREAKPOINTS;i++) {
		/* an unused slot holds address 0, which used to match and halt the
		   emulator every time the CPU reached location 0 */
		if ((breakpoints[i].addr) && (breakpoints[i].addr == addr)) {
			if (breakpoints[i].currCount) {
				breakpoints[i].currCount--;
				return 0;
			} else {
				breakpoints[i].currCount = breakpoints[i].count;
				return i+1;
			}
		}
	}
	return 0;
}


void dbgCmd_watch (int numArgs, struct args_t *args) {
	int i;

	if (numArgs < 1) {
		printf("watch  address   len  hits  break\n");
		for (i = 0; i < MAXWATCHPOINTS; i++)
			if (watchpoints[i].len)
				printf("%4d  %08x  %4u  %4u  %s\n",i,watchpoints[i].addr,
						watchpoints[i].len,watchpoints[i].hits,
						watchpoints[i].brk ? "yes" : "no");
			else
				printf("%4d  unused\n",i);
		return;
	}
	if (args[0].value >= MAXWATCHPOINTS) {
		printf("watchpoint number must be 0 to %d\n",MAXWATCHPOINTS-1);
		return;
	}
	i = args[0].value;
	if (numArgs < 2) {                      /* watch n  clears it */
		watchpoints[i].addr = 0;
		watchpoints[i].len = 0;
		watchpoints[i].hits = 0;
		watchpoints[i].brk = 0;
		printf("watchpoint %d cleared\n",i);
		return;
	}
	watchpoints[i].addr = args[1].value;
	watchpoints[i].len  = (numArgs > 2) ? args[2].value : 4;
	if (!watchpoints[i].len) watchpoints[i].len = 4;
	watchpoints[i].hits = 0;
	watchpoints[i].brk  = 0;
	printf("watchpoint %d on writes to %08x..%08x\n",i,watchpoints[i].addr,
			watchpoints[i].addr + watchpoints[i].len - 1);
}

void dbgCmd_traptrace (int numArgs, struct args_t *args) {
	if (numArgs < 1) { printf("trap tracing is %s\n",trapTrace ? "on" : "off"); return; }
	trapTrace = args[0].value ? 1 : 0;
	printf("trap tracing %s\n",trapTrace ? "on" : "off");
}

extern unsigned char ram[];

void dbgCmd_msave (int numArgs, struct args_t *args) {
	FILE * f;
	const char * name = (numArgs > 0) ? args[0].txt : "ram.bin";

	f = fopen(name,"wb");
	if (!f) { printf("cannot open %s\n",name); return; }
	fwrite(ram,1,MEM_SIZE,f);
	fclose(f);
	printf("wrote %u bytes of RAM to %s\n",(unsigned)MEM_SIZE,name);
}

void dbgCmd_history (int numArgs, struct args_t *args) {
	unsigned int n, i, idx, prev;
	char instruction[128];

	n = (numArgs > 0) ? args[0].value : 32;
	if (n > pcHistoryCount) n = pcHistoryCount;
	if (n > PCHISTORY_SIZE) n = PCHISTORY_SIZE;

	printf("last %u instructions, oldest first\n",n);
	prev = 0;
	for (i = 0; i < n; i++) {
		idx = (pcHistoryIdx - n + i) & (PCHISTORY_SIZE - 1);
		m68k_disassemble(&instruction[0], pcHistory[idx], M68K_CPU_TYPE_68010);
		/* mark anything that is not a straight line continuation, those are the
		   jumps, calls, returns and exceptions worth looking at */
		printf("%s%s %08x  %s\n",
				pcHistSuper[idx] ? "S" : "U",
				(i && (pcHistory[idx] != prev)) ? "->" : "  ",
				pcHistory[idx], instruction);
		prev = pcHistory[idx] + 2;
		{   /* advance prev past the operand words the disassembler consumed */
			unsigned int len = m68k_disassemble(&instruction[0], pcHistory[idx], M68K_CPU_TYPE_68010);
			prev = pcHistory[idx] + len;
		}
	}
}

void dbgCmd_break (int numArgs, struct args_t *args) {
	int i;

	if (numArgs == 0) {
		printf("No address  count\n");
		printf("=================\n");
		for (i=0;i<MAXBREAKPOINTS;i++) {
			if (breakpoints[i].addr) printf("%2d %08x %d\n",i,breakpoints[i].addr,breakpoints[i].count);
		}
		return;
	}
	if (numArgs == 1) {
		breakpoints[args[0].value].addr = 0;
		breakpoints[args[0].value].count = 0;
        breakpoints[args[0].value].currCount = 0;
        printf("Breakpoint %d cleared\n",args[0].value);
        return;
	}
	if ((numArgs < 2) | (numArgs > 3)) {
		printf("break requires 2 or 3 parameter\n");
		return;
	}
	if ((args[0].value < 0) || (args[0].value > MAXBREAKPOINTS-1)) {
		printf("breakpoint number not in range of 0 to %d\n",MAXBREAKPOINTS-1);
		return;
	}
	breakpoints[args[0].value].addr = args[1].value;
	if (numArgs > 2) {
        breakpoints[args[0].value].count = args[2].value;
        breakpoints[args[0].value].currCount = args[2].value;
    }
}

void dbgCmd_clr (int numArgs, struct args_t *args) {
	int i;

	for (i=0;i<MAXBREAKPOINTS;i++) {
		breakpoints[i].addr = 0;
		breakpoints[i].count = 0;
	}
}


int instructionLen (unsigned int addr) {
    char s[255];

    return (m68k_disassemble(s,addr,M68K_CPU_TYPE_68010) & DASMFLAG_LENGTHMASK);
}

unsigned int prevShownPC = 0xffffffff;

void dbgCmd_step (int numArgs, struct args_t *args) {
	int instrCount = 1;
	int i,j;
	unsigned int pc;
	unsigned int regsBefore[NUMREGS];
	unsigned int regsAfter[NUMREGS];
	char changedRegs[512];
	char buff[255];  // AD 23.10.2020: from 30 to 255 to avoid gcc10 warning

	g_busErrorCount = 0;
	pc = m68k_get_reg(NULL, M68K_REG_PC);
    if (pc != prevShownPC) showInstruction(pc,"");
	if (numArgs > 0) instrCount = args[0].value;
	if (m68k_is_stopped())
		printf("CPU is stopped (a STOP instruction executed), waiting for an interrupt;\n"
		       "stepping passes time but executes nothing until one arrives.\n");
	for (i=0; i<instrCount; i++) {
		pollBoardStatus();
		for (j=0; j<NUMREGS;j++) regsBefore[j]=m68k_get_reg(NULL, j);
		m68k_execute(1);
		sys_device_tick();
		pc = m68k_get_reg(NULL, M68K_REG_PC);
		for (j=0; j<NUMREGS;j++) regsAfter[j]=m68k_get_reg(NULL, j);
		changedRegs[0]=0;
		for (j=0; j<NUMREGS;j++) {
			if ((j != M68K_REG_PC) && (regsBefore[j] != regsAfter[j])) {
				sprintf(buff,"%s: %08x ",regs[j],regsAfter[j]);
				strcat(changedRegs,buff);
				if (j == M68K_REG_SR) {
					flagsTxt (regsAfter[j],buff); strcat(changedRegs,buff); strcat(changedRegs," ");
				}
			}
		}
		if (g_busErrorCount) {
			showBusError ("break due to");
			i = instrCount;
		}
		showInstruction(pc,changedRegs);
        prevShownPC = pc;
	}
}


void dbgCmd_rm (int numArgs, struct args_t *args) {
	unsigned int pc,pollCount;
	int maxMsgs;
	int brkpt;

	maxMsgs = args[0].value;
	if (maxMsgs<1) maxMsgs=1;
	numMsgs = 0;
	pollCount = SCC_POLL_INSTRUCTIONS;
	kb_raw();
	do {
		pollCount--;
		/* The poll counter measures instructions, and a stopped CPU executes
		 * none, so without the second test the console would go deaf exactly
		 * while the kernel sits idle waiting for a keystroke. */
		if (!(pollCount) || m68k_is_stopped()) {
			pollCount = SCC_POLL_INSTRUCTIONS;
			pollBoardStatus();
		}
		g_currPC = m68k_get_reg(NULL, M68K_REG_PC); /* REG_PPC does not work */
		m68k_execute(1);
		sys_device_tick();
		pc = m68k_get_reg(NULL, M68K_REG_PC);
	} while ((numMsgs < maxMsgs) && (!(brkpt = breakpointReached (pc))) && (!(g_ctrlCpressed)));
	kb_normal();
	if (g_busErrorCount) {
		showBusError ("break due to");
		g_busErrorCount=0;
	} else
	if (g_ctrlCpressed) {
		printf("break due to ctrl c\n");
		g_ctrlCpressed = 0;
		if (m68k_is_stopped())
			printf("CPU is stopped (a STOP instruction executed), waiting for an interrupt.\n");
	} else
		if (brkpt) printf("breakpoint %d @ %08x\n",brkpt-1,breakpoints[brkpt-1].addr);
	showInstruction(g_currPC,"");
	showInstruction(pc,"");
}


void dbgCmd_go (int numArgs, struct args_t *args) {
	unsigned int pc,pollCount;
	int brkpt;

	if (numArgs > 0)
		m68k_set_reg(M68K_REG_PC,args[0].value);

	pollCount = SCC_POLL_INSTRUCTIONS;
	kb_raw();
	do {
		pollCount--;
		/* The poll counter measures instructions, and a stopped CPU executes
		 * none, so without the second test the console would go deaf exactly
		 * while the kernel sits idle waiting for a keystroke. */
		if (!(pollCount) || m68k_is_stopped()) {
			pollCount = SCC_POLL_INSTRUCTIONS;
			pollBoardStatus();
		}
		g_currPC = m68k_get_reg(NULL, M68K_REG_PC); /* REG_PPC does not work */
		m68k_execute(1);
		sys_device_tick();
		pc = m68k_get_reg(NULL, M68K_REG_PC);
        brkpt = breakpointReached (pc);
	} while ( (brkpt == 0) && (!(g_ctrlCpressed)));
	kb_normal();
	if (g_busErrorCount) {
		showBusError ("break due to");
		g_busErrorCount=0;
	} else
	if (g_ctrlCpressed) {
		printf("break due to ctrl c\n");
		g_ctrlCpressed = 0;
	} else
		if (brkpt > 0) printf("breakpoint %d @ %08x\n",brkpt-1,breakpoints[brkpt-1].addr);
	showInstruction(g_currPC,"");
	showInstruction(pc,"");
}

/* skip over next instruction */
void dbgCmd_over (int numArgs, struct args_t *args) {
	unsigned int pc = m68k_get_reg(NULL, M68K_REG_PC);

	pc += instructionLen(pc);
	tempBreakpoint = pc;
	dbgCmd_go (numArgs,args);
}

/* generate NMI */
void dbgCmd_nmi (int numArgs, struct args_t *args) {
	m68k_cpu.nmi_pending = 1;
	dbgCmd_go (0,NULL);
}

struct devs_t {
        char name[MAXCMDLEN+1];
        int  (*proc)(int numArgs, struct args_t * args);
        };

struct devs_t devs[] =
{
    { "cmb", cmb_dbgCmd },
    { "fd",  fd_dbgCmd },
    { "pit", pit_dbgCmd },
    { "scc", scc_dbgCmd},
    { "wd",  wd_dbgCmd },
    { "cs",  cs_dbgCmd },
    { "mmu", mmu_dbgCmd },
    { "fw",  fw_dbgCmd },
    { "",    NULL}
};



void dbgCmd_device (int numArgs, struct args_t *args) {
	int i = 0;
	int found = -1;

	if (numArgs < 1) return;
	while (devs[i].name[0] != 0) {
		if (strcmp(devs[i].name,args[0].txt) == 0) {
			if (!(devs[i].proc(numArgs-1,&args[1])))
				printf("Invalid device command for device %s, try device %s help\n",args[0].txt,args[0].txt);
			found = i;
		}
		i++;
	}
	if (found == -1) {
		printf("Invalid device name. valid names are: ");
		i = 0;
		while (devs[i].name[0] != 0) {
			printf("%s ",devs[i].name);
			i++;
		}
		printf("\n");
	}
}


void dbgCmd_load (int numArgs, struct args_t *args) {
	unsigned int startAddress;
	int errs = loadSrecordFile (args[0].txt, args[1].value, &startAddress);

	if (errs) printf("%d errors\n",errs);
	if (startAddress != 0xffffffff) {
		printf("Start address %08x\n",startAddress);
		m68k_set_reg(M68K_REG_PC,startAddress);
	}
}


void dbgCmd_exec (int numArgs, struct args_t *args) {
    unsigned int loadAddr;
    unsigned int dummy;
    int errs;

    if (numArgs > 0) loadAddr = args[0].value; else loadAddr = 0x2000;
    errs = loadSrecordFile ("exec.srec", loadAddr, &dummy);
    if (errs == 0) {
        m68k_set_reg(M68K_REG_PC,loadAddr);
        printf("exec loaded\n");
    }
}

int emu_save_state(FILE * f) {
    STATEWRITEVARS("emu");
    UINT32 dummy = 0;

    STATEWRITE(id,f);
    STATEWRITELEN(g_breakOnBusError,f);
    STATEWRITELEN(g_msgBreakEnabled,f);
    STATEWRITELEN(msgClassFlags,f);
    STATEWRITELEN(msgClassFlags,f);
    STATEWRITELEN(g_showDuplicateMessages,f);
    STATEWRITELEN(dummy,f);
    STATEWRITELEN(dummy,f);
    STATEWRITELEN(dummy,f);
    STATEWRITELEN(dummy,f);

    return 1;
}

int emu_load_state(FILE * f) {
    STATEREADVARS("emu");
    UINT32 dummy;

    STATEREADID(f);
    STATEREADLEN(g_breakOnBusError,f);
    STATEREADLEN(g_msgBreakEnabled,f);
    STATEREADLEN(msgClassFlags,f);
    STATEREADLEN(msgClassFlags,f);
    STATEREADLEN(g_showDuplicateMessages,f);
    STATEREADLEN(dummy,f);
    STATEREADLEN(dummy,f);
    STATEREADLEN(dummy,f);
    STATEREADLEN(dummy,f);

    return 1;
}


void dbgCmd_img (int numArgs, struct args_t *args) {
    char filename[FILENAME_MAX+1];
    FILE * f;

    if (numArgs>1) strcpy(filename,args[1].txt); else strcpy(filename,"eagle.img");

    if (args[0].txt[0]=='s') {
        if((f = fopen(filename, "wb")) == NULL) {
            msgout (MSGC_ERR,MYSELF,MSG_NONE,"unable to create file %s",filename);
            return;
        }
        if (!(cpu_save_state(f))) { printf("save cpu failed\n"); fclose(f); unlink(filename); return; }
        if (!(cs_save_state(f))) { printf("save cs failed\n"); fclose(f); unlink(filename); return; }
        if (!(mem_save_state(f))) { printf("save mem failed\n"); fclose(f); unlink(filename); return; }
        if (!(scc_save_state(f))) { printf("save scc failed\n"); fclose(f); unlink(filename); return; }
        if (!(nv_save_state(f))) { printf("save nvram failed\n"); fclose(f); unlink(filename); return; }
        if (!(pit_save_state(f))) { printf("save pit failed\n"); fclose(f); unlink(filename); return; }
        if (!(emu_save_state(f))) { printf("save emu failed\n"); fclose(f); unlink(filename); return; }


        fclose(f);
        printf("save to %s completed.\n",filename);
    } else
    if (args[0].txt[0]=='l') {
        if((f = fopen(filename, "rb")) == NULL) {
            msgout (MSGC_ERR,MYSELF,MSG_NONE,"unable to open file %s",filename);
            return;
        }
        if (!(cpu_load_state(f))) { printf("load cpu failed\n"); fclose(f); return; }
        if (!(cs_load_state(f))) { printf("load cs failed\n"); fclose(f); return; }
        if (!(mem_load_state(f))) { printf("load mem failed\n"); fclose(f); return; }
        if (!(scc_load_state(f))) { printf("load scc failed\n"); fclose(f); return; }
        if (!(nv_load_state(f))) { printf("load nvram failed\n"); fclose(f); return; }
        if (!(pit_load_state(f))) { printf("load pit failed\n"); fclose(f); return; }
        if (!(emu_load_state(f))) { printf("load emu failed\n"); fclose(f); return; }
        fclose(f);
        printf("load from %s completed.\n",filename);
    } else
        printf ("load or save required\n");
}


void dbgCmd_int (int numArgs, struct args_t *args) {
	m68k_pulse_interrupt (args[0].value);
}

struct cmds_t cmds[] =
{
    { "an",    dbgCmd_an    , 0,1,0,"aX - change register A0 to A7"},
    { "break", dbgCmd_break , 0,3,1,"BrkNum address [count] - set breakpoint 0 to 3"},
    { "watch", dbgCmd_watch , 0,3,1,"WatchNum [address] [len] - log writes to an address"},
    { "history", dbgCmd_history , 0,1,1,"[count] - show recently executed instructions"},
    { "msave", dbgCmd_msave , 0,1,0,"[file] - dump all RAM to a file"},
    { "traptrace", dbgCmd_traptrace , 0,1,1,"[0|1] - log TRAP instructions executed in user mode"},
    { "bus"  , dbgCmd_bus   , 0,1,0,"{0|1} disable/enable break on bus error"},
    { "clr"  , dbgCmd_clr   , 0,0,0,"clear all breakpoints"},

    { "db",    dbgCmd_db    , 1,1,1,"address [count] - change/display count byte(s)"},
    { "dc",    dbgCmd_dc    , 1,1,1,"address [count] - change/display count char(s)"},
    { "device",dbgCmd_device, 1,0,0,"device (fw|wd|scc|cmb|nw) device_command"},
    { "dl",    dbgCmd_dl    , 1,1,1,"address [count] - change/display count long(s)"},
    { "dn",    dbgCmd_dn    , 0,1,0,"dX - change register D0 to D7"},
    { "dis",   dbgCmd_disass, 1,2,1,"address [num instructions] - disassemble"},
    { "dump",  dbgCmd_dump  , 2,3,1,"fromAdr len    - display memory dump"},
    { "dup" ,  dbgCmd_dup   , 0,1,0,"{0|1} disable/enable showing of duplicate messages"},
    { "dw",    dbgCmd_dw    , 1,1,1,"[count] - change/display count word(s)"},
    { "exec",  dbgCmd_exec  , 0,1,1,"[load address] - load exec"},

    { "go",    dbgCmd_go    , 0,1,1,"[address] - run, optional from address"},
    { "image", dbgCmd_img   , 0,1,0,"save|load [filename] save/load current state to/from file"},
    { "int",   dbgCmd_int   , 1,1,1,"generate interrupt n"},
    { "load",  dbgCmd_load  , 1,1,0,"filename [mem offset] - load s-record file"},
    { "nmi",   dbgCmd_nmi   , 0,0,0,"generate NMI and continue execution"},
    { "over",  dbgCmd_over  , 0,0,0,"step over next instruction"},
    { "mbreak",dbgCmd_mbrk  , 0,1,0,"set break on messages with break flag on/off" },
    { "msg",   dbgCmd_msg   , 0,0,0,"set message level, {source|all} {-|+|{+|-}warn | {+|-}err | {+|-}info}" },
    { "pc"  ,  dbgCmd_pc    , 0,1,1,"change pc"},
    { "regs",  dbgCmd_regs  , 0,0,0,"show registers"},
    { "reset", dbgCmd_rese  , 0,0,0,"reset cpu"},
    { "rm"   , dbgCmd_rm    , 0,0,0,"Run until (enabled) message from emu"},
    { "step",  dbgCmd_step  , 0,1,1,"step one or more instructions"},
    { "type",  dbgCmd_type  , 1,3,1,"fromAdr toAddr - display memory dump"},

    { "?",     dbgCmd_help  , 0,0,0,""},
    { "help",  dbgCmd_help  , 0,0,0,"show this help"},
    { "quit",  NULL         , 0,0,0,"terminate emulator"},
    { ""    ,  NULL         , 0,0,0,""}
};

void showHelp (char * caption, struct cmds_t * cmds, int extended) {
        char st[255];
        int i;

        if (caption) {
            st[0]=0;
            for (i=0;i<strlen(caption);i++) strcat(st,"=");
                printf("\n %s\n %s\n",caption,st);
        }
        i=0;
        while (cmds[i].name[0] != 0) {
            if (cmds[i].usage[0] != '\0')
                printf(" %-10s %s\n",cmds[i].name,cmds[i].usage);
            i++;
        }
        if (extended) {
            printf("args can be hex values, decimal values if started with # or register values\n");
            printf("if started with -. + at end makes value a pointer.\n");
            printf("A0+: pointer to addess 0xA0, -A0+: pointer to contents of A0\n");
            printf("-A0: contents of A0\n");
	    printf("You can break into the simulator debugger with control x or by by sending\nSIGINT to eagleemu.\n");
        }
}

void dbgCmd_help (int numArgs, struct args_t *args) {

	showHelp("Help for the 68010 emulator debugger",cmds,1);
}


/* ========================================================================
 * Convert input string to number
 * returns 1 if value found
 * pure hex
 * #xx decumal number
 * #xx+ or hex+ return the value where xx or hex points to (32 bit)
 * A0..7+, D0..7+ return the value where the register points to
 * -A0..7, -D0..7 return the register value
 * ======================================================================== */

unsigned int strToULong(char * txt, unsigned int * value) {
	char buf[255];
	int regNum = -1;
	int isPtr = 0;
	int i;

	if (xtoui(txt,value)) return 1;  /* pure hex value */
	strcpy(buf,txt);
	i = strlen(buf);
	if ((i>0) && (buf[i-1] == '+')) { isPtr = 1; buf[i-1] = 0; }
	if (buf[0]=='-') {
		if (strlen(buf) != 3)  {
			/* printf (" [Error: register required after -] "); */
			return 0;
		}
		buf[1]=toupper(buf[1]);	buf[2]=toupper(buf[2]);
		i=0;
		while((regNum == -1) & (regs[i][0] != 0)) {
			if (strcmp(&buf[1],regs[i])==0) regNum=i;
			i++;
		}
		if (regNum == -1) {
			/* printf (" [Error: Register Dx,Ax or PC required after -] "); */
			return 0;
		}
		*value = m68k_get_reg(NULL,M68K_REG_D0+regNum);
		if (isPtr) *value = m68k_read_unaligned_32 (*value);
		return 1;
	} else
	if (buf[0]=='#') {  /* decimal value */
      *value = strtol(&buf[1],NULL,10);
      if (isPtr) *value = m68k_read_unaligned_32 (*value);
	  return 1;
	}
  return 0;
}


int findAndExecCommand (	char * cmd,
                 			struct cmds_t *cmds, int numArgs,
                 			struct args_t *args) {
	int i;
	int cmdNumPartMatch = -1;
	int cmdPartMatchCount = 0;
	int cmdNum = -1;
	int allArgsAreHex = 1;
	char atleast[12];
	int j;
	int res = 0;


	for (i=0;i<numArgs;i++) {
		if (!(args[i].isValue)) allArgsAreHex = 0;
	}
	i = 0;
	/* search command */
	while ((cmdPartMatchCount < 2) && (cmdNum == -1) & (cmds[i].name[0] != 0)) {
	    if (strcmp(cmd,cmds[i].name) == 0) {
			cmdNum = i;
		} else
		if ((j = strlen(cmd)) < strlen(cmds[i].name)) {
			if (strncmp(cmd,cmds[i].name,j) == 0) {
				cmdPartMatchCount++;
				cmdNumPartMatch = i;
			}
		}
		i++;
	}

	if ((cmdNum == -1) && (cmdPartMatchCount > 1)) {
		printf("ambiguous command\n");
	} else if (cmdNum != 0xffff) {
		if (cmdNum == -1) cmdNum = cmdNumPartMatch;
		if (cmdNum >= 0) {
			if (cmds[cmdNum].minNumArgs < cmds[cmdNum].maxNumArgs)
				strcpy(atleast,"at least ");
			else
				atleast[0]=0;
			if (numArgs < cmds[cmdNum].minNumArgs) {
				if (cmds[cmdNum].minNumArgs == 1)
					printf("%s requires %sone arg\n",cmds[cmdNum].name,atleast);
				else
				    printf("%s requires %s%d args\n",cmds[cmdNum].name,atleast,cmds[cmdNum].minNumArgs);
			} else {
				if ((cmds[cmdNum].requiresAllArgsHex) && (allArgsAreHex == 0)) {
					printf("%s requires all args to be values\n",cmds[cmdNum].name);
				} else {
					if (cmds[cmdNum].proc) {
						cmds[cmdNum].proc(numArgs,args);
						res = 1;
					}
				}
			}
		} else printf("what?\n");
	}
	return res;
}

/* ========================================================================
 * command handler
 * Searches for command, checks required number of args, sets args and
 * calls routine given in cmds
 * If oneCmd is given, only that command will be executed
 * ======================================================================== */

void commandHandler(char * oneCmd)
{
	char cmd[MAXCMDLEN+1];
	int i,cmdNum;
	char * tmp;
	char * token;
	char * lastCmd = NULL;


	do {
	  for (i=0;i<MAXNUMARGS;i++) {	/* clear all args */
		  args[i].txt[0] = 0;
		  args[i].isValue = 0;
		  args[i].value = 0;
	  }
	  if (oneCmd) {
		  tmp = strdup(oneCmd);
		  printf("<dbg> %s\n",tmp);
	  } else {
      	  tmp = readline("<dbg>");		/* get command line */
	  	  if (tmp)
		    if (strlen(tmp) < 1) { free(tmp); tmp=NULL; }
	  }
	  if (tmp) {
          if (lastCmd == NULL) {
		      add_history (tmp);		/* add cmd line to histrory buffer */
		      lastCmd=strdup(tmp);
		  } else {
		      if (strcmp(tmp,lastCmd) != 0) {
				add_history(tmp);
				free(lastCmd);
				lastCmd=strdup(tmp);
			  }
		  }
		  /* split command line into args */
		  token = strtok(tmp," ");
		  numArgs = 0;
		  strncpy(cmd,token,sizeof(cmd)); cmd[sizeof(cmd)-1]=0;
		  token = strtok(NULL," ");
		  while ((token) && (numArgs < MAXNUMARGS)) {
			  strncpy(args[numArgs].txt,token,sizeof(args[numArgs].txt));
			  args[numArgs].txt[sizeof(args[numArgs].txt)-1] = 0;
			  args[numArgs].isValue = strToULong(args[numArgs].txt,&args[numArgs].value);
			  token = strtok(NULL," ");
			  numArgs++;
		  }
		  free(tmp);

		  cmdNum = -1;
		  /* check for register */
		  if (strlen(cmd) == 2) {
			  if ((toupper(cmd[0]) == 'D') && (cmd[1] >= '0') && (cmd[1] <= '7')) {
				  numArgs = 1;
				  strcpy(args[0].txt,cmd);
				  dbgCmd_andn('D',M68K_REG_D0,numArgs,args);
				  cmdNum = 0xffff;
			  } else
			  if ((toupper(cmd[0]) == 'A') && (cmd[1] >= '0') && (cmd[1] <= '7')) {
				  numArgs = 1;
				  strcpy(args[0].txt,cmd);
				  dbgCmd_andn('A',M68K_REG_A0,numArgs,args);
				  cmdNum = 0xffff;
			  }
		  }

		if (cmdNum == -1) findAndExecCommand (cmd,cmds,numArgs,args);
	  }
	  /*printf("\n");*/
	
	if (oneCmd) return;
	} while ((strcmp(cmd,"quit")));
	if (lastCmd) free(lastCmd);
}


void kbtest (void) {
	char c = 0;

	kb_raw();
	while (c != 'x') {
		if (kbhit() != 0) {
			printf(".");
			c = getch();
			fprintf(stderr,"%02x ",c);
		}
	}
	kb_normal();
	exit(1);
}

#if 0
/* timer conflicts with keyboard input ???? */
void sigtimer_action(int signo) {

}

void init_timer(void) {
	struct sigaction sigalrm_action;
	struct itimerval timer;

	timer.it_interval.tv_sec = 0;	//Deal only in usec
	timer.it_interval.tv_usec = 100;
	timer.it_value.tv_sec = 0;	//Deal only in usec
	timer.it_value.tv_usec = 100;

	sigalrm_action.sa_handler  = sigtimer_action;
	sigemptyset(&sigalrm_action.sa_mask);
	sigalrm_action.sa_flags = 0;

	sigaction(SIGALRM, &sigalrm_action, 0);
	if(setitimer(ITIMER_REAL, &timer,NULL) != 0){
		exit_error("Error starting timer");
	}
}
#endif


int main(int argc, char* argv[])
{
	int i;

	//kbtest();

	printf("eaglesim %s (%s %s)\nControl x will break into the command line\n",VER_FULLSTR,VER_COMPILE_BY,VER_COMPILE_DATE);

	/* disable all messages */
	memset(msgClassFlags,0x00,sizeof(msgClassFlags));

	/* load eagle eprom */
	if (loadROM (MEM_ROM_FILENAME))
		exit_error("unable to open rom file %s\n",MEM_ROM_FILENAME);

    m68k_set_cpu_type(M68K_CPU_TYPE_68010);
	m68k_set_int_ack_callback(sys_int_ack);

	dbgCmd_rese(0,args);  /* reset system */

	/* set signal handler to allow interrupting go command
	 * needs to be changed later because ^c is used to enter "alt load" */
	(void) signal(SIGINT,ctrlChandler);

	/*init_timer();*/

	using_history();
	read_history (HISTORY_FILENAME);

    setbuf(stdout, NULL); /* to avoid problems writing single chars w/o cr/lf */

    /* set directory for tape */
    //commandHandler("device cs directory cs/diag");

	for (i=1;i<argc;i++) {
		if ((strcmp(argv[i],"-h") == 0) || (strcmp(argv[i],"--help") == 0)) {
			printf ("usage: %s --help\n",argv[0]);
			printf ("   or  sim Command Command ..\n");
			exit(1);
		}
		commandHandler(argv[i]);
	}

	commandHandler(NULL);
	write_history (HISTORY_FILENAME);
	return 0;
}
