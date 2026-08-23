/***************************************************************************
 *  mmu.c
 *
 *  MAI 2000 Central Microprocessor Board memory management unit.
 *  See mmu.h for where this comes from.
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
 */


#include <stdio.h>
#include <string.h>
#include "m68k.h"
#include "mmu.h"
#include "sim.h"
#include "memory.h"

#define MYSELF MSG_MEM

static UINT16 mmuBase [MMU_SEGMENTS];
static UINT16 mmuLimit[MMU_SEGMENTS];
static UINT8  mmuStat [MMU_SEGMENTS];
static UINT32 mmuXlateCount;
static UINT32 mmuErrCount;
static int    mmuLoaded;        /* set once the supervisor has written a base */

void mmu_pulse_reset (void) {
    memset(mmuBase,0,sizeof(mmuBase));
    memset(mmuLimit,0,sizeof(mmuLimit));
    memset(mmuStat,0,sizeof(mmuStat));
    mmuXlateCount = 0;
    mmuErrCount = 0;
    mmuLoaded = 0;
}

int mmu_is_enabled (void) {
    return mmuLoaded;
}

/* the segment being written is selected by logical A01 A02 A03 */
static int mmu_seg_of (unsigned int address) {
    return (address >> 1) & (MMU_SEGMENTS-1);
}

void mmu_write_word (unsigned int address, unsigned int value) {
    int seg = mmu_seg_of(address);

    if (ADDR_IS_MMU_BASE(address)) {
        mmuBase[seg] = value & 0xffff;
        mmuLoaded = 1;
        /* a supervisor write initialises the segment written bit, 3.2.16.7 */
        mmuStat[seg] = 0;
        msgout (MSGC_INFO,MYSELF,MSG_WRITEW,"mmu base  seg %d = %04x (base %03x type %d%s%s) via %08x",
                seg,mmuBase[seg],mmuBase[seg] & MMU_ADDR_FIELD,
                (mmuBase[seg] & MMU_BASE_TYPE_MASK) >> MMU_BASE_TYPE_SHIFT,
                (mmuBase[seg] & MMU_BASE_R) ? " RO" : "",
                (mmuBase[seg] & MMU_BASE_X) ? " NOEXEC" : "",
                address);
    } else {
        mmuLimit[seg] = value & MMU_ADDR_FIELD;
        msgout (MSGC_INFO,MYSELF,MSG_WRITEW,"mmu limit seg %d = %03x via %08x",seg,mmuLimit[seg],address);
    }
}

void mmu_write_byte (unsigned int address, unsigned int value) {
    int seg = mmu_seg_of(address);
    UINT16 * r = ADDR_IS_MMU_BASE(address) ? &mmuBase[seg] : &mmuLimit[seg];

    /* the descriptor registers are word wide, a byte write touches half of it */
    if (address & 1) *r = (*r & 0xff00) | (value & 0xff);
    else             *r = (*r & 0x00ff) | ((value & 0xff) << 8);
    if (ADDR_IS_MMU_BASE(address)) { mmuLoaded = 1; mmuStat[seg] = 0; }
    msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"mmu %s seg %d byte write, now %04x via %08x",
            ADDR_IS_MMU_BASE(address) ? "base " : "limit",seg,*r,address);
}

/* Reads of the descriptor windows return what was written. The segment status
   register has its own read strobe on the real board (MMSTRE-) whose address
   is not given in the service manual, so it is exposed here at the odd byte of
   the limit window, which is otherwise unused. If BOSS/IX turns out to read it
   somewhere else the trace will show an unknown address and this can move. */
unsigned int mmu_read_word (unsigned int address) {
    int seg = mmu_seg_of(address);

    if (ADDR_IS_MMU_BASE(address)) return mmuBase[seg];
    return mmuLimit[seg];
}

unsigned int mmu_read_byte (unsigned int address) {
    int seg = mmu_seg_of(address);
    UINT16 v = ADDR_IS_MMU_BASE(address) ? mmuBase[seg] : mmuLimit[seg];

    if (address & 1) return v & 0xff;
    return (v >> 8) & 0xff;
}

/* Translation for an observer: same arithmetic, but it does not touch the
   segment status bits or the counters, so tracing cannot disturb what is being
   traced. Returns 0 if the access would fault. */
int mmu_peek_translate (unsigned int logical, unsigned int * phys) {
    int seg, type, viol;
    unsigned int off, base, limit, sum;

    seg   = (logical >> 21) & (MMU_SEGMENTS-1);
    off   = (logical >> 9) & MMU_ADDR_FIELD;
    base  = mmuBase[seg] & MMU_ADDR_FIELD;
    limit = mmuLimit[seg] & MMU_ADDR_FIELD;
    type  = (mmuBase[seg] & MMU_BASE_TYPE_MASK) >> MMU_BASE_TYPE_SHIFT;

    viol = 0;
    switch (type) {
        case MMU_TYPE_ABSENT:   viol = 1; break;
        case MMU_TYPE_LIMIT_GE: viol = (off >= limit); break;
        case MMU_TYPE_LIMIT_LT: viol = (off <  limit); break;
        case MMU_TYPE_LIMIT_LE: viol = (off <= limit); break;
    }
    if (viol) return 0;
    sum = (base + off) & MMU_ADDR_FIELD;
    *phys = (sum << 9) | (logical & 0x1ff);
    return 1;
}

int mmu_translate (unsigned int logical, int isWrite, unsigned int * phys) {
    int seg, type, viol;
    unsigned int off, base, limit, sum;

    /* in user mode the segment number comes from A21 A22 A23 */
    seg  = (logical >> 21) & (MMU_SEGMENTS-1);
    off  = (logical >> 9) & MMU_ADDR_FIELD;     /* logical A09 through A20 */
    base = mmuBase[seg] & MMU_ADDR_FIELD;
    limit = mmuLimit[seg] & MMU_ADDR_FIELD;
    type = (mmuBase[seg] & MMU_BASE_TYPE_MASK) >> MMU_BASE_TYPE_SHIFT;

    viol = 0;
    switch (type) {
        case MMU_TYPE_ABSENT:   viol = 1; break;                /* ABSEG-  */
        case MMU_TYPE_LIMIT_GE: viol = (off >= limit); break;    /* SEGDC1- */
        case MMU_TYPE_LIMIT_LT: viol = (off <  limit); break;    /* SEGDC2- */
        case MMU_TYPE_LIMIT_LE: viol = (off <= limit); break;    /* SEGDC3- */
    }
    if (viol) {
        mmuStat[seg] |= MMU_ST_LIMITERR;
        mmuErrCount++;
        msgout (MSGC_ERR,MYSELF,MSG_NONE,"mmu %s violation, seg %d off %03x limit %03x type %d, logical %08x",
                (type == MMU_TYPE_ABSENT) ? "absent segment" : "limit",seg,off,limit,type,logical);
        return 0;
    }
    if (isWrite && (mmuBase[seg] & MMU_BASE_R)) {
        mmuStat[seg] |= MMU_ST_WRITEERR;
        mmuErrCount++;
        msgout (MSGC_ERR,MYSELF,MSG_NONE,"mmu write to read only seg %d, logical %08x",seg,logical);
        return 0;
    }

    /* physical A09 through A20 is base plus logical, three 4 bit adders */
    sum = (base + off) & MMU_ADDR_FIELD;
    *phys = (sum << 9) | (logical & 0x1ff);
    if (isWrite) mmuStat[seg] |= MMU_ST_WRITTEN;
    mmuXlateCount++;
    return 1;
}

/******************************************************************************
 * debugger command
 ******************************************************************************/

void mmu_showRegs (int numArgs, struct args_t *args) {
    int i, type;
    static const char * typeName[4] = {"absent","addr>=lim","addr<lim","addr<=lim"};

    printf("mmu %s, %u translations, %u faults\n",
            mmuLoaded ? "loaded" : "never written to",mmuXlateCount,mmuErrCount);
    printf("seg  base  limit  attr        maps logical           to physical\n");
    for (i = 0; i < MMU_SEGMENTS; i++) {
        type = (mmuBase[i] & MMU_BASE_TYPE_MASK) >> MMU_BASE_TYPE_SHIFT;
        printf(" %d   %03x   %03x   %-9s %s%s  %08x..%08x  %06x\n",
                i,mmuBase[i] & MMU_ADDR_FIELD,mmuLimit[i] & MMU_ADDR_FIELD,
                typeName[type],
                (mmuBase[i] & MMU_BASE_R) ? "RO " : "rw ",
                (mmuBase[i] & MMU_BASE_X) ? "NX" : "x ",
                i << 21, (i << 21) | 0x1fffff,
                (unsigned int)((mmuBase[i] & MMU_ADDR_FIELD) << 9));
    }
    printf("status:");
    for (i = 0; i < MMU_SEGMENTS; i++) printf(" %d:%x",i,mmuStat[i]);
    printf("   (1 written 2 execerr 4 writeerr 8 limiterr)\n");
}

void mmu_help (int numArgs, struct args_t *args);

struct cmds_t mmuCmds[] =
{
    { "registers",  mmu_showRegs,   0,0,0,"show the eight segment descriptors"},
    { "?",          mmu_help,       0,0,0,""},
    { "help",       mmu_help,       0,0,0,"show this help"},
    { "",  NULL, 0,0,0,""}
};

void mmu_help (int numArgs, struct args_t *args) {
    showHelp ("mmu help commands",mmuCmds,0);
}

int mmu_dbgCmd (int numArgs, struct args_t * args) {
    return findAndExecCommand (args[0].txt,mmuCmds,numArgs-1,&args[1]);
}
