/***************************************************************************
 *  fourway.c
 *
 *  MAI 2000/3000 4-Way controller. See fourway.h for the addressing and
 *  M8155A for the protocol.
 *
 *  Stage one: answer the probe, run the initialisation handshake, and log
 *  every access so that what BOSS/IX does next can be read off a trace rather
 *  than guessed at.
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
#include "fourway.h"
#include "sim.h"
#include "memory.h"

#define MYSELF MSG_FW

typedef enum {
    FW_INIT_CBLOCK_LOW = 0,     /* expecting the command block address, LSW  */
    FW_INIT_CBLOCK_HIGH,        /* then its most significant part            */
    FW_INIT_VECTOR,             /* then the base interrupt vector, one write */
    FW_INIT_DONE
} fw_init_t;

typedef struct {
    int        installed;
    UINT8      status;
    UINT8      instr;           /* last value written to the instruction reg */
    fw_init_t  initState;
    UINT32     cmdBlock;        /* word address, as handed over              */
    UINT32     intVector;
    UINT32     accesses;
} fw_regs_t;

static fw_regs_t fw[FW_MAX];
static int fwDsrBits = FW_ST_DSRA | FW_ST_DSRB | FW_ST_DSRC | FW_ST_DSRD;

/* Per the interrupt table in BFISD 8079 section 3.2.6, levels 2 and 4 both
 * carry "System I/O bus peripheral device controllers" and are the only two
 * that can be bus vectored, which is what a board supplying its own vector
 * needs. The Winchester controller sits on 2 and the cartridge streamer on 4,
 * so the 4-Way is one of those two and the manual does not say which. Start on
 * 4 and let "device fw level <n>" move it, so the question can be settled by
 * measurement rather than by guessing twice. */
static int fwIntLevel = 4;
static int fwIntPending = 0;    /* board index + 1, or 0 for none */
static int fwIntVector  = 0;    /* vector for the pending interrupt         */

/* The manual calls the DSR bits "negative TRUE" without saying which way that
 * comes out in the register, so this starts with all four asserted, meaning a
 * terminal on every port, and "device fw dsr <value>" flips it without a
 * rebuild if the trace says otherwise. */

static void fw_complete (int n, int port, int condition);
static void fw_runCommand (int n, int port);

int fw_decode (unsigned int address) {
    unsigned int base = address & FW_ADDR_MASK;
    int i;

    for (i = 0; i < FW_MAX; i++)
        if (base == (unsigned int)(FW_BASE_FIRST + i * FW_BASE_STEP)) {
            if (fw[i].installed) return i;
            return -1;                  /* absent board still bus errors */
        }
    return -1;
}

static const char * fw_regName (unsigned int address) {
    switch (address & FW_REG_MASK) {
        case FW_REG_STATUS: return "status";
        case FW_REG_INSTR : return "instruction";
    }
    return "unknown";
}

static void fw_decodeStatus (UINT8 st, char * tx) {
    char txt[255];

    txt[0] = 0; tx[0] = 0;
    if (st & FW_ST_BUSY)       strcat(txt,"BUSY ");
    if (st & FW_ST_SELFTEST)   strcat(txt,"SELFTEST ");
    if (st & FW_ST_NOCMDBLOCK) strcat(txt,"NOCMDBLK ");
    if (st & FW_ST_NOVECTOR)   strcat(txt,"NOVECTOR ");
    if (st & FW_ST_DSRA)       strcat(txt,"DSRA ");
    if (st & FW_ST_DSRB)       strcat(txt,"DSRB ");
    if (st & FW_ST_DSRC)       strcat(txt,"DSRC ");
    if (st & FW_ST_DSRD)       strcat(txt,"DSRD ");
    if (txt[0]) { txt[strlen(txt)-1] = 0; sprintf(tx,"[%s]",txt); }
}


/* Read a word out of host memory. Everything the board is handed is a word
   address, shifted right once, the same convention the disk DMA and the
   streamer IOPB use. */
static UINT16 fw_peek (UINT32 byteAddr) {
    return sys_read_word(byteAddr,1) & 0xffff;
}

static void fw_poke (UINT32 byteAddr, UINT16 value) {
    sys_write_word(byteAddr,value,1);
}

/* Command complete. The board owns an interrupt vector handed to it during
   initialisation and raises it when it has finished a command block. */
static void fw_complete (int n, int port, int condition) {
    if (fw[n].instr & FW_IR_INTINHIBIT) {
        msgout (MSGC_INFO,MYSELF,MSG_NONE,"fw%d: completion interrupt inhibited",n);
        return;
    }
    /* base plus four per channel plus the condition, M8155A 3.2.2.2 */
    fwIntVector = (fw[n].intVector + port * 4 + condition) & 0xff;
    msgout (MSGC_INFO,MYSELF,MSG_NONE,
            "fw%d port%c: completion interrupt, level %d, vector %02x (base %02x + %d)",
            n,'A'+port,fwIntLevel,fwIntVector,fw[n].intVector,port*4+condition);
    fwIntPending = n + 1;
    m68k_set_int_line (fwIntLevel, ASSERT_LINE);
}

int fw_irq_ack (int level) {
    int n;

    if ((level != fwIntLevel) || (!fwIntPending)) return M68K_INT_ACK_SPURIOUS;
    n = fwIntPending - 1;
    fwIntPending = 0;
    msgout (MSGC_INFO,MYSELF,MSG_NONE,"fw%d: interrupt acknowledged, handing back vector %02x",n,fwIntVector);
    m68k_set_int_line (fwIntLevel, CLEAR_LINE);
    return fwIntVector;
}

static void fw_runCommand (int n, int port) {
    fw_regs_t * b = &fw[n];
    UINT32 cb, packet;
    UINT16 cmd, count;
    UINT32 i;
    UINT8  ch;

    cb = (b->cmdBlock << 1) + port * FW_CB_PORTSIZE;
    cmd    = fw_peek(cb + FW_CB_CMD);
    count  = fw_peek(cb + FW_CB_COUNT);
    packet = (((UINT32)fw_peek(cb + FW_CB_ADDR_HI)) << 16) | fw_peek(cb + FW_CB_ADDR_LO);
    packet <<= 1;

    msgout (MSGC_FUNC,MYSELF,MSG_NONE,
            "fw%d port%c: command block %08x, cmd %04x, count %d, packet %08x",
            n,'A'+port,cb,cmd,count,packet);

    if (count && (count < 64)) {
        char hex[256]; char *q = hex; UINT32 k;
        for (k = 0; (k < count) && (k < 32); k++)
            q += sprintf(q,"%02x ",sys_read_byte(packet+k,1) & 0xff);
        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"fw%d port%c: packet: %s",n,(char)(65+port),hex);
    }

    switch (cmd & 0xff) {
        case FW_CMD_DT:
        case FW_CMD_SINGL:
            /* Data transfer, host to port. Emit it where the SCC console
               output goes so a terminal on port A of the first board shows up
               without any further plumbing. */
            for (i = 0; i < count; i++) {
                ch = sys_read_byte(packet + i,1) & 0xff;
                if ((n == 0) && (port == 0)) {
                    fputc(ch & 0x7f,stderr);
                }
            }
            if ((n == 0) && (port == 0)) fflush(stderr);
            msgout (MSGC_INFO,MYSELF,MSG_NONE,"fw%d port%c: transferred %d bytes",n,'A'+port,count);
            fw_poke(cb + FW_CB_STATUS,FW_ST_EXECUTED);
            fw_complete(n,port,FW_VEC_CMDEXECUTED);
            break;

        default:
            msgout (MSGC_NOTIMP,MYSELF,MSG_NONE,
                    "fw%d port%c: command %02x not implemented, reporting success anyway",
                    n,'A'+port,cmd & 0xff);
            fw_poke(cb + FW_CB_STATUS,FW_ST_EXECUTED);
            fw_complete(n,port,FW_VEC_CMDEXECUTED);
            break;
    }
}

/* the initialisation handshake, M8155A 3.3.6 */
static void fw_instrWrite (int n, unsigned int value) {
    fw_regs_t * b = &fw[n];

    b->instr = value & 0xff;

    switch (b->initState) {
        case FW_INIT_CBLOCK_LOW:
            b->cmdBlock = value & 0xffff;
            b->initState = FW_INIT_CBLOCK_HIGH;
            b->status |= FW_ST_BUSY;    /* held until the Z80 has taken it */
            msgout (MSGC_FUNC,MYSELF,MSG_NONE,"fw%d: command block address, low %04x",n,value & 0xffff);
            break;

        case FW_INIT_CBLOCK_HIGH:
            b->cmdBlock |= ((UINT32)(value & 0xff)) << 16;
            b->initState = FW_INIT_VECTOR;
            b->status |= FW_ST_BUSY;
            b->status &= ~FW_ST_NOCMDBLOCK;
            msgout (MSGC_FUNC,MYSELF,MSG_NONE,
                    "fw%d: command block address complete, word %06x, byte address %08x",
                    n,b->cmdBlock,b->cmdBlock << 1);
            break;

        case FW_INIT_VECTOR:
            b->intVector = value & 0xff;
            b->initState = FW_INIT_DONE;
            b->status |= FW_ST_BUSY;
            b->status &= ~FW_ST_NOVECTOR;
            msgout (MSGC_FUNC,MYSELF,MSG_NONE,
                    "fw%d: initialised, command block %08x, vector %02x",
                    n,b->cmdBlock << 1,b->intVector);
            break;

        case FW_INIT_DONE: {
            int port;

            if (value & FW_IR_PORTRESET)
                msgout (MSGC_INFO,MYSELF,MSG_NONE,"fw%d: reset of port %d",n,
                        (value & FW_IR_PORTSEL_MASK) >> FW_IR_PORTSEL_SHIFT);
            for (port = 0; port < FW_PORTS; port++)
                if (value & (FW_IR_PORTA_READY << port))
                    fw_runCommand(n,port);
            break;
        }
    }
}

unsigned int fw_read_byte (unsigned int address, int flags) {
    int n = fw_decode(address);
    fw_regs_t * b;
    UINT8 value;
    char tx[255];

    if (n < 0) { BUSERROR(flags,address,MSG_READB); return 0xff; }
    b = &fw[n];
    b->accesses++;

    if ((address & FW_REG_MASK) == FW_REG_STATUS) {
        value = b->status | fwDsrBits;
        fw_decodeStatus(value,tx);
        msgout (MSGC_INFO,MYSELF,MSG_READB,"fw%d: %08x status %02x %s",n,address,value,tx);
        /* BUSY is set when the board is handed something and clears once it
           has taken it. Clearing it on the read after it was set gives the
           host the set then clear transition the manual describes without
           ever leaving it stuck. */
        b->status &= ~FW_ST_BUSY;
        return value;
    }

    msgout (MSGC_NOTIMP,MYSELF,MSG_READB,"fw%d: read of %08x (%s), returning 0",n,address,fw_regName(address));
    return 0;
}

unsigned int fw_read_word (unsigned int address, int flags) {
    return fw_read_byte(address,flags) & 0xff;
}

void fw_write_byte (unsigned int address, unsigned int value, int flags) {
    int n = fw_decode(address);

    if (n < 0) { BUSERROR(flags,address,MSG_WRITEB); return; }
    fw[n].accesses++;

    if ((address & FW_REG_MASK) == FW_REG_INSTR) {
        msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"fw%d: %08x instruction register <= %02x",n,address,value & 0xff);
        fw_instrWrite(n,value);
        return;
    }
    msgout (MSGC_NOTIMP,MYSELF,MSG_WRITEB,"fw%d: write %02x to %08x (%s)",n,value & 0xff,address,fw_regName(address));
}

void fw_write_word (unsigned int address, unsigned int value, int flags) {
    int n = fw_decode(address);

    if (n < 0) { BUSERROR(flags,address,MSG_WRITEW); return; }
    fw[n].accesses++;

    if ((address & FW_REG_MASK) == FW_REG_INSTR) {
        msgout (MSGC_INFO,MYSELF,MSG_WRITEW,"fw%d: %08x instruction register <= %04x",n,address,value & 0xffff);
        fw_instrWrite(n,value);
        return;
    }
    msgout (MSGC_NOTIMP,MYSELF,MSG_WRITEW,"fw%d: write %04x to %08x (%s)",n,value & 0xffff,address,fw_regName(address));
}

void fw_pulse_reset (void) {
    int i;

    memset(&fw,0,sizeof(fw));
    for (i = 0; i < FW_MAX; i++) {
        if (i < FW_INSTALLED) fw[i].installed = 1;
        /* out of reset the board has neither a command block address nor a
           vector, and says so, M8155A 3.3.6 step 2 */
        fw[i].status = FW_ST_NOCMDBLOCK | FW_ST_NOVECTOR;
        fw[i].initState = FW_INIT_CBLOCK_LOW;
    }
    fwIntPending = 0;
    m68k_set_int_line (fwIntLevel, CLEAR_LINE);
}

/******************************************************************************
 * debugger commands
 ******************************************************************************/

static const char * fw_initName (fw_init_t s) {
    switch (s) {
        case FW_INIT_CBLOCK_LOW : return "waiting for command block low";
        case FW_INIT_CBLOCK_HIGH: return "waiting for command block high";
        case FW_INIT_VECTOR     : return "waiting for interrupt vector";
        case FW_INIT_DONE       : return "initialised";
    }
    return "?";
}

void fw_showRegs (int numArgs, struct args_t *args) {
    int i;
    char tx[255];

    for (i = 0; i < FW_MAX; i++) {
        if (!fw[i].installed) continue;
        fw_decodeStatus(fw[i].status | fwDsrBits,tx);
        printf("fw%d at %08x, instruction register at %08x\n"
               "     status: %02x %s\n"
               "      state: %s\n"
               "  cmd block: %08x   vector: %06x\n"
               "   accesses: %u\n",
               i,FW_BASE_FIRST + i*FW_BASE_STEP,FW_BASE_FIRST + i*FW_BASE_STEP + FW_REG_INSTR,
               fw[i].status | fwDsrBits,tx,
               fw_initName(fw[i].initState),
               fw[i].cmdBlock << 1,fw[i].intVector,
               fw[i].accesses);
    }
}

void fw_dsr (int numArgs, struct args_t *args) {
    if (numArgs < 1) { printf("fw dsr bits: %02x\n",fwDsrBits); return; }
    fwDsrBits = args[0].value & 0xf0;
    printf("fw dsr bits set to %02x\n",fwDsrBits);
}

void fw_level (int numArgs, struct args_t *args) {
    if (numArgs < 1) { printf("fw interrupt level: %d\n",fwIntLevel); return; }
    fwIntLevel = args[0].value & 7;
    printf("fw interrupt level set to %d\n",fwIntLevel);
}

void fw_help (int numArgs, struct args_t *args);

struct cmds_t fwCmds[] =
{
    { "registers", fw_showRegs, 0,0,0,"show the 4-way boards"},
    { "dsr",       fw_dsr,      0,1,1,"[bits] - set the DSR bits in the status register"},
    { "level",     fw_level,    0,1,1,"[n] - set the interrupt level, 2 or 4"},
    { "?",         fw_help,     0,0,0,""},
    { "help",      fw_help,     0,0,0,"show this help"},
    { "",  NULL, 0,0,0,""}
};

void fw_help (int numArgs, struct args_t *args) {
    showHelp ("fw help commands",fwCmds,0);
}

int fw_dbgCmd (int numArgs, struct args_t * args) {
    return findAndExecCommand (args[0].txt,fwCmds,numArgs-1,&args[1]);
}
