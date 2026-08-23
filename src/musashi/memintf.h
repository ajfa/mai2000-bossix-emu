#ifndef MEMINTF__H
#define MEMINTF__HEADER

#include "m68000.h"



/*typedef int (*device_irq_callback)(device_t *device, int irqnum);*/

/* to be defined outsite of musashi */

void fatalerror(char * msg, ...);

void debugger_instruction_hook(void * device, unsigned int pc);

unsigned int read8(unsigned int address);
unsigned int read16(unsigned int address);
unsigned int read32(unsigned int address);

/* used for what ? */
#define readimm16 read16

/* Memory access for the disassembler */
unsigned int read_disassembler8  (unsigned int address);
unsigned int read_disassembler16 (unsigned int address);
unsigned int read_disassembler32 (unsigned int address);

void write8(unsigned int address, unsigned int value);
void write16(unsigned int address, unsigned int value);
void write32(unsigned int address, unsigned int value);
void cpu_pulse_reset(void);

#endif /* SIM__HEADER */
