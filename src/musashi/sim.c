#include "sim.h"
#include "memintf.h"

void fatalerror(char * msg, ...) {
}

void debugger_instruction_hook(void * device, unsigned int pc) {
}

unsigned int read8(unsigned int address) {
		return 0;
}

unsigned int read16(unsigned int address) {
	return 0;
}

unsigned int read32(unsigned int address) {
		return 0;
}



/* Memory access for the disassembler */
unsigned int read_disassembler8  (unsigned int address) {
	return 0;
}

unsigned int read_disassembler16 (unsigned int address) {
	return 0;
}

unsigned int read_disassembler32 (unsigned int address) {
	return 0;
}


void write8(unsigned int address, unsigned int value) {
}

void write16(unsigned int address, unsigned int value) {
}

void write32(unsigned int address, unsigned int value) {
}

void cpu_pulse_reset(void) {
}

int main () {
}


