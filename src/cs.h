/***************************************************************************
 *   cs.h
 *
 *  Created: Nov, 22 2011
 *  Changed: Dec, 20 2011
 *  Armin Diehl <ad@ardiehl.de>
 ****************************************************************************/
/*
 * 2000 cs (1/4" cardridge streamer controller)
 * I have no documentation about cs or ts
 */
#ifndef CS_H
#define CS_H

#include "sim.h"

/*          diags access is at 4d00000 */
#define CS_ADDR             0x00d00000
#define CS_ADDR_MASK        0x00FF0000
#define CS_ADDR_REGMASK     0x0000000F
#define CS_GO_REG           0x0
#define CS_STATUS_REG       0x8
#define CS_FILEMASK         "%sF%05d"
#define CS_DEFTAPEDIR       "cs/diag/"
#define CS_BLOCKSIZE        512
#define CS_TAPESIZE_MB_DEF  120
#define CS_TAPESIZE_MB_MIN  5
#define CS_TAPESIZE_BKLS(MB) (MB * 1024 * 1024 / CS_BLOCKSIZE)
/* dont know if it is 2 or 4 (both are vectored) */
#define CS_INTNO            4
#define CS_DEF_INTVEC       0xC0

typedef struct {
    UINT32 nextBlockPtr;
    UINT16 flags_opcode;
    UINT32 bufferPtr;
    UINT32 bufferLen;       /* seems to be in words */
    UINT16 numBlocks;
    UINT16 unused;
    UINT16 unit;
    UINT16 status;
    UINT32 resDataLen;
    UINT16 status1;
    UINT16 status2;
    UINT16 status3;
} cs_iopb_t;

#define FILEMASK_MAX 30

#define STAT_CMD_VER 1

typedef struct {
    unsigned int unknown1;    /* 8 */
    unsigned int IOPB_addr;   /* 32 */
    unsigned int firstIOPB_addr;
    unsigned int unknown2;    /* 16 */
    unsigned int lastCommand; /* 16 */
    int    statWriteCount;
    /*  check done while booting:
        move.w  (A2), D0  ; A2=d00000
        andi.w  #$c00, D0
        cmpi.w  #$c00, D0 */
    /* values for register read */
    UINT16 statusReg;        /* 16 */
    UINT16 goReg;            /* 16 */
    int statCmd;
    int execCount;
    int fileNo;
    int intVector;
    int intCount;
    char directory[FILENAME_MAX+1];
    char filemask[FILEMASK_MAX];
    char currFilename[FILENAME_MAX+1];
    int isReadonly;
    FILE * f;
    int fileIsOpen;
    int fileIsOpenForWrite;
    int fileBlockNo;
    int fileRemainingBlocks;
    int numBlocksOnTape;
    int tapesizeMB;
} cs_regs_t;

#define CS_STATREG_DEFAULT 0x0000

/* have no clue about the bits, 0c00 is expected, 0x0000 is assumed */
#define CS_GOREG_DEFAULT   0x0c00
#define CS_GOREG_BUSY      0x0000

#define ADDR_IS_CS(ADDR) ((ADDR & CS_ADDR_MASK) == CS_ADDR)

/* status register values */
#define CS_STATREG_OK           0
#define CS_STATREG_BUSERR       0x0200
#define CS_STATREG_SELFFAIL1    0x1000
#define CS_STATREG_SELFFAIL2    0x2000
#define CS_STATREG_SELFFAIL3    0x4000


/* from mcsfs */
/* IOPB PRIMARY STATUS BIT MASKS (HEX): */
#define CS_STAT_TAPERROR   0x0001
#define CS_STAT_NODRIVE    0x0002
#define CS_STAT_PWRFAILRES 0x0004  /* Power fail reset */
#define CS_STAT_WROTEEOT   0x0008  /* Wrote to EOT */
#define CS_STAT_UNERAAPP   0x0010  /* Unerased area in append */
#define CS_STAT_RWABORT    0x0020
#define CS_STAT_WRPROT     0x0040
#define CS_STAT_FILENMARK  0x0080  /* File mark detected */
#define CS_STAT_NOCARD     0x0100  /* No cartridge */
#define CS_STAT_BADXFER    0x0200
#define CS_STAT_READEOT    0x0400
#define CS_STAT_FILLERSENT 0x0800
#define CS_STAT_CHAINTERM  0x1000  /* Chain processing terminated */
#define CS_STAT_IPBPERR    0x2000  /* Iopb paramater error */
#define CS_STAT_EOD        0x4000  /* End of data = End of Tape */
#define CS_STAT_COMPLETE   0x8000


/*
ROBLEM: A problem has been reported when loading the 7.4B MCS logic test on
          the SPX system. The error reads:

          "UNEXPECTED INTERRUPT 2C"
*/

/* IOPB SECONDARY STATUS WORD (status1) 1 BIT MASKS */
/* after rewind on real machine: 9088: CS_STAT1_STAT0BYTE | CS_STAT1_WRITEPROT | CS_STAT1_STAT1BIT | CS_STAT1_BOM */   

#define CS_STAT1_RESET     0x0001 /* Power on/reset occurred */         
#define CS_STAT1_EOM       0x0002 /* End of recorded media */
#define CS_STAT1_BUSERR    0x0004 /* Bus Parity error */
#define CS_STAT1_BOM       0x0008 /* Beginning of media */
#define CS_STAT1_MBLOCK    0x0010 /* Marginal block detected */
#define CS_STAT1_NODATA    0x0020 /* No data detected */
#define CS_STAT1_ILLCMD    0x0040 /* Illegal command */
#define CS_STAT1_STAT1BIT  0x0080 /* Status byte 1 bits */
#define CS_STAT1_FILEMARK  0x0100 /* File mark detected */
#define CS_STAT1_BADBLOCK  0x0200 /* Bad block not located */
#define CS_STAT1_DATAERR   0x0400 /* Unrecoverable data error */
#define CS_STAT1_EORM      0x0800 /* End of recorded media */
#define CS_STAT1_WRITEPROT 0x1000 /* Write protected cart. */
#define CS_STAT1_UNSEL     0x2000 /* Unselected drive/uninstalled cartridge */
#define CS_STAT1_NOCARDR   0x4000 /* Cartrigde not in place */
#define CS_STAT1_STAT0BYTE 0x8000 /* Status in 0 bytes */

/* IOPB OPCODE BYTES */

#define CS_CMD_INIT        0x00 /* Initialize */
                                /* data buffer may contain int vector */
#define CS_CMD_SKIP        0x01 /* Skip forward */
#define CS_CMD_REWIND      0x02 /* Rewind */
#define CS_CMD_RETENSION   0x03 /* Retension */
#define CS_CMD_ERASE       0x04 /* Erase */
#define CS_CMD_STATUS      0x05 /* Read drive status */
#define CS_CMD_TEST        0x06 /* Run self test */
#define CS_CMD_READ        0x10 /* Read */
#define CS_CMD_WRITE       0x11 /* Write */
#define CS_CMD_APPEND      0x12 /* Append */

/* IOPB FLAG BYTE BIT MASKS */

#define CS_CMDF_OPTIONAL   0x0100 /* Enb. optional command */
#define CS_CMDF_INT        0x0200 /* Enb. intrpts. */
#define CS_CMDF_TESTWRITE  0x0400 /* Enb. write on self-test */
#define CS_CMDF_STREAM     0x0800 /* Enb. streaming */
#define CS_CMDF_FILEMARKS  0x4000 /* Enb. filemarks */



/*
Boot device: cs
ERROR read16 unknown memory area 00d00000 (PC:0040156c)

System file: 


Booting from cs0ERROR write16 8000 to unknown memory 00d00008 (PC:0040165c)
ERROR read16 unknown memory area 00d00000 (PC:0040163c)
ERROR write16 0316 to unknown memory 00d00008 (PC:004016e0)
ERROR read16 unknown memory area 00d00000 (PC:0040163c)
ERROR read16 unknown memory area 00d00000 (PC:0040163c)
ERROR write16 8001 to unknown memory 00d00008 (PC:004016f2)
ERROR read16 unknown memory area 00d00000 (PC:0040163c)
ERROR write16 0000 to unknown memory 00d00000 (PC:004016fc)

 */

unsigned int cs_read_byte(unsigned int address, int flags);
unsigned int cs_read_word(unsigned int address, int flags);
void cs_write_byte(unsigned int address, unsigned int value, int flags);
void cs_write_word(unsigned int address, unsigned int value, int flags);
void cs_pulse_reset(void);
int cs_dbgCmd(int numArgs, struct args_t * args);
void cs_processContinue(void);  /* called each n instructions */
int  cs_irq_ack(int level);
int cs_save_state(FILE * f);
int cs_load_state(FILE * f);

#endif
