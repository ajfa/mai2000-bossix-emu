/***************************************************************************
 *   wd.h
 *
 *  Created: Nov, 22 2011
 *  Changed: Dec, 25 2020
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
 * mai 2000/3000 winchester disk (wd) controller definitions
 * see M8158A '5 1/4" Winchester Disk Drive Controller Service Manual'
 */

#ifndef WD_H
#define WD_H

#include "sim.h"

//define WD_DISABLE

#define WD0_ADDR		0xCC0000
#define WD1_ADDR		0xCD0000
#define WD_ADDR_MASK		0xFFFF0000


#define WD0_INSTALLED   1
#define WD1_INSTALLED   0
#define WD_MAX          2
#define WD_SCSICMD_MAX  255
#define WD_SECTOR_SIZE  512

#define ADDR_IS_WD0(ADDR) ((ADDR & WD_ADDR_MASK) == WD0_ADDR)
#define ADDR_IS_WD1(ADDR) ((ADDR & WD_ADDR_MASK) == WD1_ADDR)

// the loader from the install tape accesses the controller e.g. via cc8007
// tested on 2000, only the lower 4 bits will be decoded ccfff7 is equal to cc0007
#define WD_ADDR_TO_REG(A)	(A & 0x0f)
#define WD_PHASE_COUNT      5

/* dont know if it is 2 or 4 (both are vectored) */
#define WD_INTNO            2


/* These registers are loaded one byte at a time, with the 1's complement
   of the system address, right shifted once */
#define WD_REG_DMA_HI		0x01
#define WD_REG_DMA_MID		0x02
#define WD_REG_DMA_LOW		0x03

/* interrupt vector register */
#define WD_REG_INTVEC		0x04

/* int vector register ?? Test reads from this port and requires value written to 0x04 ?? */
#define WD_REG_INTVEC2      0x05

/* Read/Write Control Register */
#define WD_REG_CTL2		0x07
/* bits in control register */
#define WD_CTL_SRST     0x01    /* SSRST+ Reset the wdc. Minimum 25 miliseconds, logical or'ed with POR and PFD */
#define WD_CTL_LED      0x02	/* LED- */
#define WD_CTL_INTEN    0x04    /* INTEN+ Enable operation complete and bus error interrupts */
#define WD_CTL_SEQEN    0x08    /* SEQEN+ Enable DMA */
#define WD_CTL_INTEND0  0x10    /* INTEND0+ Enable Drive 0 completion Interrupt */
#define WD_CTL_INTEND1	0x20
#define WD_CTL_INTD0	0x40    /* INTD0+ Enable Drive 0 seek completion interrupt */
#define WD_CTL_INTD1	0x80


/* This address byte is written to by the host during I/O data transfer
   (Information transfer phase, host to controller) phase */
#define WD_HOST_WRITE		0x08

/* Status Read register */
#define WD_REG_STAT		0x09
/* status bits */
#define WD_STAT_BUSERR      0x01    /* + Bus error during wds's bus mastership */
#define WD_STAT_OUTEMPTY    0x02    /* + Output data register empty */
#define WD_STAT_OPCOMP      0x04    /* + Operation complete */
#define WD_STAT_INPFULL     0x08    /* + Input register full */
#define WD_STAT_SRESET      0x10    /* + SCSI bus in reset state */
#define WD_STAT_SMSG        0x20    /* + SCSI bus in message state */
#define WD_STAT_SBUSY       0x40    /* + SCSI bus busy */
#define WD_STAT_SCMD        0x80    /* + SCSI bus in command,status or message phase */

#define WD_REG_SELECT		0x0A
#define WD_REG_READINP		0x0B
#define WD_REG_CLRBUSERR	0x0C

#define SCSI_TESTREADY      0x00
#define SCSI_REZEROUNIT     0x01
#define SCSI_REQUESTSENSE   0x03
#define SCSI_FORMAT         0x04
#define SCSI_READ           0x08
#define SCSI_WRITE          0x0A
#define SCSI_SEEK           0x0B
#define SCSI_TRANSLATE      0x0F
#define SCSI_WRITEBUF       0x13
#define SCSI_READBUFRAM     0x14
#define SCSI_MODESELECT     0x15
#define SCSI_MODESENSE      0x1A
#define SCSI_STARTSTOP      0x1B
#define SCSI_RECDIAG        0x1C
#define SCSI_SENDDIAG       0x1D
#define SCSI_READCAPACITY   0x25
#define SCSI_READ2          0x28
#define SCSI_WRITE2         0x2A
#define SCSI_WRITEVERIFY    0x2E
#define SCSI_VERIFY         0x2F
#define SCSI_SEARCH         0x31

/* controller registers */
typedef struct {
    UINT32 dmaAddress;
    UINT8 intVector;
    //UINT8 intVectorError;
    UINT8 statusReg;
    UINT8 scsiCheck;
    UINT8 selected;
    UINT8 readInpReg;
    //UINT8 ctlReg;
    UINT8 hostWriteReg;
    UINT8 scsiBuf[WD_SCSICMD_MAX];
    UINT8 ctlReg2; /* 07 ?? */
    int scsiIdx;
    int installed;
    int state;
    int stateCounter;
    /* non scsi commands */
    int cmdCounter;
    int currCommand;
    UINT16 dmaTestWordRead;
    UINT8  dmaTestCount;    /* 2=HI(dmaTestWordRead),1=LO, 0=none */
    UINT32 intCount;
    int    intPending;     /* a completion interrupt is outstanding */
    int    intAsserted;    /* current state of the request line   */
    UINT32 replyBytesLeft;
    UINT32 replyBytePos;
    UINT8  replyBuffer[WD_SECTOR_SIZE];
    /* disk image backing store, added to make the drive real */
    FILE * img;
    char   imgName[FILENAME_MAX+1];
    UINT32 imgBlocks;      /* size of the image in 512 byte blocks */
    int    imgReadonly;
    UINT8  sense[4];       /* sense bytes returned by REQUEST SENSE  */
    UINT8  statusByte;     /* SCSI status handed over in the status phase */
    UINT8  dataBuf[WD_SECTOR_SIZE*8];

} wd_regs_t;

typedef enum {
    SCSI_S_IDLE,
    SCSI_S_MESSAGE,
    SCSI_S_BUSY,
    SCSI_S_COMPLETE,
    SCSI_S_READRESULTS,     /* data in phase,  status 0x48 */
    SCSI_S_STATUS,          /* status phase,   status 0xcc */
    SCSI_S_MESSAGEBYTE,     /* message phase,  status 0xe8 */
    SCSI_S_ENDOFCOMMAND
} SCSI_S;


typedef enum {
    CMD_RESET=0,
    CMD_SCSIRESET=3,
    CMD_RESET_OUTREGFULL,
    CMD_PROCESS_SCSICMD,
    CMD_SET_INPFULL
} CMD_S;


unsigned int wd_read_byte(unsigned int address, int flags);
unsigned int wd_read_word(unsigned int address, int flags);
void wd_write_byte(unsigned int address, unsigned int value, int flags);
void wd_write_word(unsigned int address, unsigned int value, int flags);
void wd_pulse_reset(void);
int wd_dbgCmd(int numArgs, struct args_t * args);
int wd_attach_image(int unit, const char * name);
int wd_units_ready(void);
void wd_processContinue(void);  /* called each n instructions */
int  wd_irq_ack(int level);

int wd_save_state(FILE * f);
int wd_load_state(FILE * f);

#endif
