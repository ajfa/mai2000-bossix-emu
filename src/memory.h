/* memory dispatcher for eagle emulation
 * memory read/write requests
 * ROM read requests
 * no MMU
 */

#ifndef MEMORY_H
#define MEMORY_H

#define MEM_BOARDSIZE		0x40000		/* 256K per board */
#define MEM_BOARDS			6
#define MEM_SIZE			MEM_BOARDSIZE * MEM_BOARDS

#define MEM_ADDR_ROM		0x400000
#define MEM_ADDR_ROM_MASK	0xFFF00000
#define MEM_ROMSIZE			0x4000		/* 2 times 8k ROM */
#define MEM_ROMEND          (MEM_ADDR_ROM+MEM_ROMSIZE-1)
#define MEM_MAX_RAMROM      MEM_ROMEND
#define MEM_MEMEND			0x200000-1	/* max 2MB Ram */
#define ADDR_IS_RAM(ADDR) (ADDR < MEM_SIZE)
#define ADDR_IS_RAMSPACE(ADDR) (ADDR < MEM_MEMEND)
#define ADDR_IS_ROM(ADDR) ((ADDR & MEM_ADDR_ROM_MASK) == MEM_ADDR_ROM)
#define MEM_ROM_FILENAME	"mai2000rom.bin"

/* Read/write macros */
#define READ_BYTE(BASE, ADDR) (BASE)[ADDR]
#define READ_WORD(BASE, ADDR) (((BASE)[ADDR]<<8) |			\
						  (BASE)[(ADDR)+1])
#define READ_LONG(BASE, ADDR) (((BASE)[ADDR]<<24) |			\
							  ((BASE)[(ADDR)+1]<<16) |		\
							  ((BASE)[(ADDR)+2]<<8) |		\
							  (BASE)[(ADDR)+3])
/* aaggghh: this was a typo in musashi 3.31, it was % instead of & in WRITE_BYTE */
#define WRITE_BYTE(BASE, ADDR, VAL) (BASE)[ADDR] = (VAL)&0xff
#define WRITE_WORD(BASE, ADDR, VAL) (BASE)[ADDR] = ((VAL)>>8) & 0xff;		\
									(BASE)[(ADDR)+1] = (VAL)&0xff
#define WRITE_LONG(BASE, ADDR, VAL) (BASE)[ADDR] = ((VAL)>>24) & 0xff;		\
									(BASE)[(ADDR)+1] = ((VAL)>>16)&0xff;	\
									(BASE)[(ADDR)+2] = ((VAL)>>8)&0xff;		\
									(BASE)[(ADDR)+3] = (VAL)&0xff

int mem_isValidForRead (unsigned int address);
int mem_isValidForWrite (unsigned int address);



unsigned int mem_read_byte(unsigned int address, int flags);
unsigned int mem_read_word(unsigned int address, int flags);

void mem_write_byte(unsigned int address, unsigned int value, int flags);
void mem_write_word(unsigned int address, unsigned int value, int flags);

/* for setting ROM to 0x0 */
void mem_pulse_reset(void);
int loadROM (char * filename);

int mem_save_state(FILE * f);
int mem_load_state(FILE * f);

#endif
