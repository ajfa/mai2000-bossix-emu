/***************************************************************************
 * fd.h
 *
 *  Created: Dec, 12 2011
 *  Changed: May, 06 2015
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
 * mai 2000 floppy controller (wd1793 + 2 or 8k buffer)
 */

#ifndef FD_H
#define FD_H

#include "sim.h"

/*
 70XXXX WRITE Floppy address counter reset (13J)
 72XXXX READ Floppy status (read only)
 76XXXX WRITE Floppy control latch WRITE
 78XXXX Both Floppy controller chip select
 7AXXXX Both Floppy buffer READ/WRITE
 * */

#define FD_ADDR         0x700000
#define FD_ADDR_MASK    0xF00000
#define FD_ADDR_ARMASK  0x0F0000
// floppy disk controller section options (13J)
#define FD_ADDR_FLPOPT  0x700000
#define FLPOPT_BUFWR 0
#define FLPOPT_CMD   1
#define FLPOPT_ENBINTR 2
#define FLPOPT_ENBDRQ  3
#define FLPOPT_HD_SD   4  /* 1=HD, 0=SD */
#define FLPOPT_FM_MFM  5  /* 1=FM, 0=MFM */
/* bit 6 is not connected */
#define FLPOPT_FRES    7  /* Reset incl wd1793 reset */

// Status Transfer Control (13L) (read)
#define FD_ADDR_FLPSTAT 0x720000
#define FLPSTAT_BUSY 0
#define FLPSTAT_IDXA 1
#define FLPSTAT_ENINTR 2
#define FLPSTAT_ENDRQ 3
#define FLPSTAT_INTRA 4
#define FLPSTAT_DRQA 5
// bit 6 is NC
#define FLPSTAT_FRST 7


#define FD_ADDR_CONTROL 0x740000

// write only floppy control latch (13k)
#define FD_ADDR_FLPCONT 0x760000
#define FLPCONT_SEL0 0
#define FLPCONT_SEL1 1
#define FLPCONT_MOTOR0 2
#define FLPCONT_MOTOR1 3
#define FLPCONT_DLOCK0 4
#define FLPCONT_DLOCK1 5
#define FLPCONT_PRECOMP 6
#define FLPCONT_SIDE 7

#define FD_INTNO 3

// 125

// 386

#define FD_ADDR_WD       0x780000
#define FD_ADDR_BUFFER   0x7A0000


#define ADDR_IS_FD(ADDR) ((ADDR & FD_ADDR_MASK) == FD_ADDR)

#define FD_AREA(ADDR) ((ADDR & FD_ADDR_ARMASK) >> 16)
#define FD_FLPOPT 0x00
// write=cmd, read=status
#define FD_STAT   0x02
#define FD_CONT   0x06
#define FD_WD1793 0x08
#define FD_BUFF   0x0A

//#define FD8K
/* this may be a 2K (default) or 8K ram */
#ifndef FD8K
#define FD_BUFFER_SIZE  2048
#define FD_BUFFER_MASK  0x7ff
#else
#define FD_BUFFER_SIZE  8192
#define FD_BUFFER_MASK  0x1fff
#endif

#define FD_MAX_REGISTERS 5

typedef struct {
    UINT8  regs[FD_MAX_REGISTERS];
    UINT32 bufferPos;
    unsigned char buffer[FD_BUFFER_SIZE];
    UINT8 flpopt_13J;      /* write 13J floppy disk controller section options */
    UINT8 flpstat_13L;     /* read 13L  Status Transfer Control */
    UINT8 flpcont_13K;     /* write 13K Floppy control (Sel,Motor,Precomp,Side,Doorlock) */
    char flpBuf[FD_BUFFER_SIZE];
	UINT8  cmdRunning;
    INT8   lastStepDirection;  // 0 -1 or +1
    UINT8  currTrack;
	UINT8  intFlags;
    
    UINT32 dummy2;
    UINT32 dummy3;
    UINT32 dummy4;
} fd_t;



#define WD1793_CMD     0
#define WD1793_STAT    0
#define WD1793_TRACK   1
#define WD1793_SECTOR  2
#define WD1793_DATA    3


#define WD1793_R_CMD     0
#define WD1793_R_TRACK   1
#define WD1793_R_SECTOR  2
#define WD1793_R_DATA    3
#define WD1793_R_STAT    4

/* WD1793 commands */
#define WD179X_RESTORE               0x00   /* Type I */
#define WD179X_SEEK                  0x10   /* Type I */
#define WD179X_STEP                  0x20   /* Type I */
#define WD179X_STEP_U                0x30   /* Type I */
#define WD179X_STEP_IN               0x40   /* Type I */
#define WD179X_STEP_IN_U             0x50   /* Type I */
#define WD179X_STEP_OUT              0x60   /* Type I */
#define WD179X_STEP_OUT_U            0x70   /* Type I */
#define WD179X_READ_REC              0x80   /* Type II */
#define WD179X_READ_RECS             0x90   /* Type II */
#define WD179X_WRITE_REC             0xA0   /* Type II */
#define WD179X_WRITE_RECS            0xB0   /* Type II */
#define WD179X_READ_ADDR             0xC0   /* Type III */
#define WD179X_FORCE_INTR            0xD0   /* Type IV */
#define WD179X_READ_TRACK            0xE0   /* Type III */
#define WD179X_WRITE_TRACK           0xF0   /* Type III */

// params to fd_genInterrupt
#define WD1793_CMD_START    0
#define WD1793_CMD_COMPLETE 1
#define WD1793_INDEX        2
#define WD1793_IMMEDIATE    3

// int flags for type IV commands
#define WD1793_INT_READY             1
#define WD1793_INT_NOTREADY          (1 << 1)
#define WD1793_INT_INDEX             (1 << 2)
#define WD1793_INT_IMMEDIATE         (1 << 3)

// exec times
#define FD_SEEK_EXEC_TIME 10
#define FD_RW_EXEC_TIME 10

// bit positions in cmd
#define WS1793_CF_VERIFY 2
#define WS1793_CF_MOTOR  3
#define WS1793_CF_UPDATE 4
#define WS1793_CF_MULTSEC 4
#define WS1793_CF_SIDECOMP 3
#define WS1793_CF_E 2
#define WS1793_CF_C 1
#define WS1793_CF_P 0


                             // Common status bits
#define FLG_BUSY     0x01    // Controller is executing a command
#define FLG_READONLY 0x40    // The disk is write-protected
#define FLG_NOTREADY 0x80    // The drive is not ready

                             // Type-1 command status
#define FLG_INDEX    0x02    // Index mark detected
#define FLG_TRACK0   0x04    // Head positioned at track #0
#define FLG_CRCERR   0x08    // CRC error in ID field
#define FLG_SEEKERR  0x10    // Seek error, track not verified
#define FLG_HEADLOAD 0x20    // Head loaded

                             // Type-2 and Type-3 command status
#define FLG_DRQ      0x02    // Data request pending
#define FLG_LOSTDATA 0x04    // Data has been lost (missed DRQ)
 
#define FLG_ERRCODE  0x18    // Error code bits
#define FLG_BADDATA  0x08    // 1 = bad data CRC
#define FLG_NOTFOUND 0x10    // 2 = sector not found
#define FLG_BADID    0x18    // 3 = bad ID field CRC
#define FLG_DELETED  0x20    // Deleted data mark (when reading)
#define FLG_WRFAULT  0x20    // Write fault (when writing)

#define C_DELMARK  0x01
#define C_SIDECOMP 0x02
#define C_STEPRATE 0x03
#define C_VERIFY   0x04
#define C_WAIT15MS 0x04
#define C_LOADHEAD 0x08
#define C_SIDE     0x08
#define C_IRQ      0x08
#define C_SETTRACK 0x10
#define C_MULTIREC 0x10

#define S_DRIVE    0x03
#define S_RESET    0x04
#define S_HALT     0x08
#define S_SIDE     0x10
#define S_DENSITY  0x20


unsigned int fd_read_byte(unsigned int address, int flags);
unsigned int fd_read_word(unsigned int address, int flags);
void fd_write_byte(unsigned int address, unsigned int value, int flags);
void fd_write_word(unsigned int address, unsigned int value, int flags);
void fd_pulse_reset(void);
int fd_dbgCmd(int numArgs, struct args_t * args);

int fd_save_state(FILE * f);
int fd_load_state(FILE * f);

void fd_processContinue(void);  /* called each n instructions */
void fd_genInterrupt(int kind);

#endif
