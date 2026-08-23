/***************************************************************************
 *  mmu.h
 *
 *  MAI 2000 Central Microprocessor Board memory management unit.
 *
 *  Implemented from BFISD 8079 'MAI 2000 Series Service Manual', section
 *  3.2.16, and confirmed against the addresses BOSS/IX actually touches.
 *
 *  Eight variable length segments per user. Each segment has a 12 bit base,
 *  a 12 bit limit, 4 attribute bits and 4 status bits. The MMU translates
 *  logical A09 through A20, A01 through A08 pass straight through, which is
 *  what sets the 512 byte minimum segment size.
 *
 *  Segment descriptors are written by the supervisor to two I/O windows:
 *      8XXXXX  base  descriptor, signal MMBWE-
 *      AXXXXX  limit descriptor, signal MMLWE-
 *  and the segment being written is selected by logical A01, A02 and A03.
 *  In user mode the segment is selected by A21, A22 and A23 instead, which is
 *  how translation happens without the program knowing.
 ****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MMU_H
#define MMU_H

#include "sim.h"

#define MMU_SEGMENTS        8

#define MMU_BASE_ADDR       0x800000
#define MMU_LIMIT_ADDR      0xA00000
#define MMU_ADDR_MASK       0xF00000

#define ADDR_IS_MMU_BASE(A)  (((A) & MMU_ADDR_MASK) == MMU_BASE_ADDR)
#define ADDR_IS_MMU_LIMIT(A) (((A) & MMU_ADDR_MASK) == MMU_LIMIT_ADDR)
#define ADDR_IS_MMU(A)       (ADDR_IS_MMU_BASE(A) || ADDR_IS_MMU_LIMIT(A))

/* base descriptor bits, see 3.2.16.5 and 3.2.16.6 */
#define MMU_BASE_R          0x8000      /* segment is read only              */
#define MMU_BASE_X          0x4000      /* segment is data only, no execute  */
#define MMU_BASE_TYPE_MASK  0x3000      /* limit comparison policy           */
#define MMU_BASE_TYPE_SHIFT 12
#define MMU_ADDR_FIELD      0x0fff      /* A20 through A09                   */

/* TYPE field, decoded by the 74S139 at 4T */
#define MMU_TYPE_ABSENT     0           /* ABSEG-,  always an error          */
#define MMU_TYPE_LIMIT_GE   1           /* SEGDC1-, error if addr >= limit   */
#define MMU_TYPE_LIMIT_LT   2           /* SEGDC2-, error if addr <  limit   */
#define MMU_TYPE_LIMIT_LE   3           /* SEGDC3-, error if addr <= limit   */

/* segment status bits, readable by the supervisor, see 3.2.16.7 */
#define MMU_ST_WRITTEN      0x01        /* D01, segment has been written to  */
#define MMU_ST_EXECERR      0x02        /* D02, execute error                */
#define MMU_ST_WRITEERR     0x04        /* D03, write error                  */
#define MMU_ST_LIMITERR     0x08        /* D04, limit or absent segment      */

void mmu_pulse_reset (void);
void mmu_write_word (unsigned int address, unsigned int value);
void mmu_write_byte (unsigned int address, unsigned int value);
unsigned int mmu_read_word (unsigned int address);
unsigned int mmu_read_byte (unsigned int address);

/* returns 1 if the access is allowed and stores the physical address in phys,
   returns 0 and leaves the status bits set if it is a management violation */
int mmu_translate (unsigned int logical, int isWrite, unsigned int * phys);
int mmu_peek_translate (unsigned int logical, unsigned int * phys);

int mmu_is_enabled (void);
int mmu_dbgCmd (int numArgs, struct args_t * args);

#endif
