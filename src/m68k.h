/***************************************************************************
 *  m68l.h
 *
 *  Created: Dec, 10 2011
 *  Changed: Dec, 21 2011
 *  Armin Diehl <ad@ardiehl.de>
 * 
 * Compatibility routines to use musashi 4.x (from mame) with 3.3 like
 * interfaces
 ****************************************************************************/

#ifndef M68K_H
#define M68K_H

#include "musashi/m68kcpu.h"
#include "musashi/m68000.h"

#define cpu_read_byte read8
#define cpu_read_word read16
#define cpu_read_long read32
#define cpu_write_byte write8
#define cpu_write_word write16
#define cpu_write_long write32

/* there is another 3.32 with different names ?? */
#define m68k_read_memory_8 read8
#define m68k_read_memory_16 read16
#define m68k_read_memory_32 read32
#define m68k_write_memory_8 write8
#define m68k_reite_memory_16 write16
#define m68k_write_memory_32 write32



/* Registers used by m68k_get_reg() and m68k_set_reg() */
typedef enum
{
        /* Real registers */
        M68K_REG_D0,            /* Data registers */
        M68K_REG_D1,
        M68K_REG_D2,
        M68K_REG_D3,
        M68K_REG_D4,
        M68K_REG_D5,
        M68K_REG_D6,
        M68K_REG_D7,
        M68K_REG_A0,            /* Address registers */
        M68K_REG_A1,
        M68K_REG_A2,
        M68K_REG_A3,
        M68K_REG_A4,
        M68K_REG_A5,
        M68K_REG_A6,
        M68K_REG_A7,
        M68K_REG_PC,            /* Program Counter */
        M68K_REG_SR,            /* Status Register */
        M68K_REG_SP,            /* The current Stack Pointer (located in A7) */
        M68K_REG_USP,           /* User Stack Pointer */
        M68K_REG_ISP,           /* Interrupt Stack Pointer */
        M68K_REG_MSP,           /* Master Stack Pointer */
        M68K_REG_SFC,           /* Source Function Code */
        M68K_REG_DFC,           /* Destination Function Code */
        M68K_REG_VBR,           /* Vector Base Register */
        M68K_REG_CACR,          /* Cache Control Register */
        M68K_REG_CAAR,          /* Cache Address Register */

        /* Assumed registers */
        /* These are cheat registers which emulate the 1-longword prefetch
         * present in the 68000 and 68010.
         */
        M68K_REG_PREF_ADDR,     /* Last prefetch address */
        M68K_REG_PREF_DATA,     /* Last prefetch data */

        /* Convenience registers */
        M68K_REG_PPC,           /* Previous value in the program counter */
        M68K_REG_IR,            /* Instruction register */
        M68K_REG_CPU_TYPE       /* Type of CPU being run */
} m68k_register_t;



void m68k_set_cpu_type(int cpu_type);

/* Pulse the RESET pin on the CPU.
 * You *MUST* reset the CPU at least once to initialize the emulation
 * You *MUST* call m68k_set_cpu_type() before resetting
 *       the CPU for the first time
 */

void m68k_pulse_reset(void);

/* execute num_cycles worth of instructions.  returns number of cycles used */
int m68k_execute(int num_cycles);


/* Peek at the internals of a CPU context.  This can either be a context
 * retrieved using m68k_get_context() or the currently running context.
 * If context is NULL, the currently running CPU context will be used.
 */
unsigned int m68k_get_reg(void* context, m68k_register_t reg);

/* Poke values into the internals of the currently running CPU context */
void m68k_set_reg(m68k_register_t reg, unsigned int value);

/* Check if an instruction is valid for the specified CPU type */
/*
unsigned int m68k_is_valid_instruction(unsigned int instruction, unsigned int cpu_type);
*/

/* Disassemble 1 instruction using the epecified CPU type at pc.  Stores
 * disassembly in str_buff and returns the size of the instruction in bytes.
 */
unsigned int m68k_disassemble(char* str_buff, unsigned int pc, unsigned int cpu_type);

/* prototypes for 4.x functions (macros in mamedefs.h) */
CPU_INIT( m68000 );
CPU_INIT( m68010 );
CPU_RESET( m68k );
CPU_EXECUTE( m68k );

/* ======================================================================== */
/* ============================== CALLBACKS =============================== */
/* ======================================================================== */

/* These functions allow you to set callbacks to the host when specific events
 * occur.  Note that you must enable the corresponding value in m68kconf.h
 * in order for these to do anything useful.
 * Note: I have defined default callbacks which are used if you have enabled
 * the corresponding #define in m68kconf.h but either haven't assigned a
 * callback or have assigned a callback of NULL.
 */

/* Set the callback for an interrupt acknowledge.
 * You must enable M68K_EMULATE_INT_ACK in m68kconf.h.
 * The CPU will call the callback with the interrupt level being acknowledged.
 * The host program must return either a vector from 0x02-0xff, or one of the
 * special interrupt acknowledge values specified earlier in this header.
 * If this is not implemented, the CPU will always assume an autovectored
 * interrupt, and will automatically clear the interrupt request when it
 * services the interrupt.
 * Default behavior: return M68K_INT_ACK_AUTOVECTOR.
 */
void m68k_set_int_ack_callback(int  (*callback)(device_t *device, int int_level));


/* Set the callback for a breakpoint acknowledge (68010+).
 * You must enable M68K_EMULATE_BKPT_ACK in m68kconf.h.
 * The CPU will call the callback with whatever was in the data field of the
 * BKPT instruction for 68020+, or 0 for 68010.
 * Default behavior: do nothing.
 */
void m68k_set_bkpt_ack_callback(void (*callback)(device_t *device, unsigned int data));


/* Set the callback for the RESET instruction.
 * You must enable M68K_EMULATE_RESET in m68kconf.h.
 * The CPU calls this callback every time it encounters a RESET instruction.
 * Default behavior: do nothing.
 */
void m68k_set_reset_instr_callback(void  (*callback)(device_t * device));

void m68k_set_instr_callback(void  (*callback)(device_t * device, unsigned int pc));

void m68k_pulse_interrupt (int level);
void m68k_set_int_line (int level, int state);

int cpu_save_state(FILE * f);
int cpu_load_state(FILE * f);

#endif /* M68K__HEADER */
