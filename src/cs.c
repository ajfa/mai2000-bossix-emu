/***************************************************************************
 *  cs.c
 *
 *  Created: Nov, 22 2011
 *  Changed: Dec, 20 2011
 *  Armin Diehl <ad@ardiehl.de>
 ****************************************************************************/
/*
 * mai 2000/3000 1/4" cardridge steamer
 *
 * Current status:
 * Implemented using information from the mcsfs program included on the diag
 * tape, i have no documentation about the tape interface at all.
 * Booting the diagnostics from tape works
 * Passes all mcs tests except bus error test:
 * 
 * Magtape Cartridge Streamer Logic Test  Rev C2           18:01:23  03/31/88
 * <mcs>run
 * Test 1  -->  Status register addressing
 * Test 2  -->  Go register addressing
 * Test 3  -->  Reset
 * Test 4  -->  Read status
 * Test 5  -->  IOPB processing
 * Test 6  -->  Rewind
 * Test 7  -->  Rewind interrupt
 * Test 8  -->  DMA integrity
 * Test 9  -->  Write/read one record
 * Test 10 -->  Write/read one record interrupt
 * Test 11 -->  Write/read shoeshine
 * Test 12 -->  On-the-fly write/read shoeshine
 * On-The-Fly Tests Skipped Due To Controller Firmware Revision. . .
 * Test 13 -->  Write/read multiple records ramp-up
 * Test 14 -->  File addressing
 * Test 15 -->  Append
 * Test 16 -->  Streaming write/read
 * Test 17 -->  Interrupt-driven streaming write/read
 * Test 18 -->  On-The-Fly Streaming write/read
 * On-The-Fly Tests Skipped Due To Controller Firmware Revision. . .
 * Test 19 -->  Bozo parameters
 * Test 20 -->  Bus-master parity error
 * 
 * Parity Error Expected But Not Received                              0006818  14
 *     Address Of Bad Parity    
 *     060076                   
 * <SP> - Continue, <ESC> - Abort, <.> - No Pause 
 * 
 * 
 * End Of Pass 1   Total Test Errors 1
 * 
 * 
 * General function
 * There will be IO parameter blocks generated in memory, the address of this
 * block will be set by writing to the status register. Writing to the go
 * register will start the operation. Multiple IOPB's may be chained.
 */

#include <stdio.h>
#include "m68k.h"
#include "cs.h"
#include "sim.h"
#include "memory.h"
#include "util.h"
#include <sys/stat.h>
#include <unistd.h>

#define MYSELF MSG_CS


#define CS_REG_IOPB_H 0

cs_regs_t cs;

/* memory flags, 1=debug, no bus error (for now) */
#define IOPB_MF 1



void tape_file_name(int fileNum, char * filename) {
    sprintf(filename,cs.filemask,cs.directory,fileNum+1);
}

void tape_count_numBlocks(void) {
    char filename[FILENAME_MAX+1];
    int fileNo = 0;
    int fsize;

    cs.numBlocksOnTape = 0;

    tape_file_name(fileNo,filename);
    while (file_exists(filename)) {
        fsize = file_getSize(filename);
        if (fsize > 0) cs.numBlocksOnTape += fsize / CS_BLOCKSIZE;
        fileNo++;
        tape_file_name(fileNo,filename);
    }
}

int tape_file_exists(int fileNo) {
    char filename[FILENAME_MAX+1];
    tape_file_name(fileNo,filename);
    return file_exists(filename);
}

int tape_erase(void) {
    char filename[FILENAME_MAX+1];
    int fileNo = 0;
    if (cs.isReadonly) return 0;

    cs.numBlocksOnTape = 0;
    msgout (MSGC_FUNC,MYSELF,MSG_NONE,"erasing tape");
    tape_file_name(fileNo,filename);
    while (file_exists(filename)) {
        if (unlink(filename) != 0) return 0;
        fileNo++;
        tape_file_name(fileNo,filename);
    }
    return 1;
}


void tape_close_file(void) {
    if ((cs.fileIsOpen) || (cs.fileIsOpenForWrite)) {
        fclose(cs.f);
        cs.fileIsOpen = 0;
        cs.fileIsOpenForWrite = 0;
        cs.fileBlockNo = 0;
        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"%s closed",cs.currFilename);
        cs.currFilename[0] = 0;
    }
}

void tape_create_empty_file(int fileNo) {
    char filename[FILENAME_MAX+1];

    tape_file_name(fileNo,filename);
    tape_close_file();
    if (file_exists(filename)) return;
    if((cs.f = fopen(filename, "wb")) == NULL) {
        msgout (MSGC_ERR,MYSELF,MSG_NONE,"unable to create empty file %s",filename);
        return;
    }
    fclose(cs.f);
}

int tape_is_filemark(int fileNo) {
    char filename[FILENAME_MAX+1];

    tape_file_name(fileNo,filename);
    if (!(file_exists(filename))) return 0;
    if (file_getSize(filename) == 0) return 1;
    return 0;
}

/* are we at end of tape ? */
int tape_at_eod(int fileno) {
    if (!(tape_file_exists(cs.fileNo))) return 1;

    if (tape_is_filemark(cs.fileNo+1))
        if (!(tape_file_exists(cs.fileNo+2))) return 1;

    return 1;
}
    

int tape_open_fileR(int fileNo) {
    char filename[FILENAME_MAX+1];
    struct stat statbuf;
    tape_file_name(fileNo,filename);
    tape_close_file();
    if (!(file_exists(filename))) return 0;

    if((cs.f = fopen(filename, "rb")) == NULL) return 0;
    cs.fileIsOpen = 1;
    cs.fileBlockNo = 0;
    strcpy(cs.currFilename,filename);
    
    stat(filename,&statbuf);
    cs.fileRemainingBlocks = statbuf.st_size / CS_BLOCKSIZE;
    return 1;
}

//address seems to be words also, otherwise stack will be overwritten
int tape_read(cs_iopb_t * iopb) {
    char buf[CS_BLOCKSIZE];
    int i,j,res,count;
    int blocksToRead;
    unsigned int address = iopb->bufferPtr << 1;
    /* int blocksToRead = iopb->numBlocks; this is wrong, the buffer length is of interest */
    int blocksToReadRequested = (iopb->bufferLen * 2) / CS_BLOCKSIZE;
    blocksToRead = blocksToReadRequested;
    if (blocksToRead > cs.fileRemainingBlocks) blocksToRead = cs.fileRemainingBlocks;
    iopb->numBlocks = 0; count = 0;
    for (i=0;i<blocksToRead;i++) {
        if ((res = fread(&buf,1,CS_BLOCKSIZE,cs.f)) != CS_BLOCKSIZE) {
            iopb->status = CS_STAT_TAPERROR;
            iopb->status1 = CS_STAT1_DATAERR;
            msgout (MSGC_ERR,MYSELF,MSG_NONE,"fread from %s returned %d",cs.currFilename,res);
            return 0;
        }
        /* save to memory */
        for (j=0;j<CS_BLOCKSIZE;j++) {
            sys_write_byte(address, buf[j], 1);  /* TODO: handle bus errors */
            address++;
        }
        iopb->numBlocks++;
        cs.fileRemainingBlocks--;
        count++;
    }
    iopb->status = CS_STAT_COMPLETE;
    iopb->status1 = 0;
#if 0
    if (blocksToReadRequested >= blocksToRead) {
        iopb->status1 |= CS_STAT1_FILEMARK; /* correct for end of file ? */
        iopb->status = CS_STAT_FILENMARK;
    }
#endif
    /* should we update the block count ? */
    /* no, test 11 will fail then iopb->bufferLen = count * CS_BLOCKSIZE; */
    if (cs.fileRemainingBlocks == 0) {
        if (tape_file_exists(cs.fileNo+1)) {
            iopb->status1 |= CS_STAT1_FILEMARK; /* correct for end of file ? */
            iopb->status = CS_STAT_FILENMARK;
            msgout (MSGC_FUNC,MYSELF,MSG_NONE,"all blocks read from file, returning CS_STAT_FILENMARK");
        } else {
            iopb->status = CS_STAT_READEOT | CS_STAT_CHAINTERM;
            iopb->status1 = CS_STAT1_EOM;
            msgout (MSGC_FUNC,MYSELF,MSG_NONE,"all blocks read from file and we are at end of tape, returning CS_STAT_READEOT");
        }
    }
    msgout (MSGC_FUNC,MYSELF,MSG_NONE,"%d blocks read from #%d to %08x (bufLen bytes: #%d)",count,cs.fileNo+1,iopb->bufferPtr << 1,iopb->bufferLen * 2);
    return count;
}


void tape_write(cs_iopb_t * iopb) {
    char filename[FILENAME_MAX+1];
    char buf[CS_BLOCKSIZE];
    int i,j,res;
    unsigned int address = iopb->bufferPtr << 1;
    int blocksToWrite = (iopb->bufferLen * 2) / CS_BLOCKSIZE;

        
    if(!cs.fileIsOpenForWrite) {
        tape_file_name(cs.fileNo,cs.currFilename);
        if((cs.f = fopen(cs.currFilename, "wb")) == NULL) {
            msgout (MSGC_ERR,MYSELF,MSG_NONE,"failed to create %s",filename);
            cs.currFilename[0]=0;
            iopb->status = CS_STAT_TAPERROR;
            iopb->status2 = CS_STAT1_DATAERR;
            return;
        }
        cs.fileIsOpenForWrite = 1;
    }
    iopb->numBlocks = 0;
    for (i=0;i<blocksToWrite;i++) {
        /* get from memory */
        for (j=0;j<CS_BLOCKSIZE;j++) {
            buf[j] = sys_read_byte(address, 1);  /* TODO: handle bus errors */
            address++;
        }
        /* write to file */
        if ((res = fwrite(&buf,1,CS_BLOCKSIZE,cs.f)) != CS_BLOCKSIZE) {
            iopb->status = CS_STAT_TAPERROR;
            iopb->status1 = CS_STAT1_DATAERR;
            fflush(cs.f);
            msgout (MSGC_ERR,MYSELF,MSG_NONE,"fwrite to %s returned %d",cs.currFilename,res);
            return;
        }
        cs.numBlocksOnTape++;
        cs.fileBlockNo++;
        iopb->numBlocks++;
    }
    iopb->status = CS_STAT_COMPLETE;
    fflush(cs.f);
    msgout (MSGC_FUNC,MYSELF,MSG_NONE,"%d blocks written from %08x to %s",iopb->numBlocks,iopb->bufferPtr << 1,cs.currFilename);
    if (iopb->flags_opcode & CS_CMDF_FILEMARKS) {
        tape_close_file();
        cs.fileNo++;
        tape_create_empty_file(cs.fileNo);
        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"filemark flag set, closing file %d and setting next file %d",cs.fileNo,cs.fileNo+1);
    }
}


void getIOPB(unsigned int address, cs_iopb_t * iopb) {
    iopb->nextBlockPtr = sys_read_long(address,IOPB_MF); address+=4; 
    iopb->flags_opcode = sys_read_word(address,IOPB_MF); address+=2;
    iopb->bufferPtr = sys_read_long(address,IOPB_MF); address+=4;
    iopb->bufferLen = sys_read_long(address,IOPB_MF); address+=4;
    iopb->numBlocks = sys_read_word(address,IOPB_MF); address+=2;
    iopb->unused = sys_read_word(address,IOPB_MF); address+=2;
    iopb->unit = sys_read_word(address,IOPB_MF); address+=2;
    iopb->status = sys_read_word(address,IOPB_MF); address+=2;
    iopb->resDataLen = sys_read_long(address,IOPB_MF); address+=4;
    iopb->status1 = sys_read_word(address,IOPB_MF); address+=2;
    iopb->status2 = sys_read_word(address,IOPB_MF); address+=2;
    iopb->status3 = sys_read_word(address,IOPB_MF);
}

void putIOPB (unsigned int address, cs_iopb_t * iopb) {
    sys_write_long(address,iopb->nextBlockPtr,IOPB_MF); address+=4;
    sys_write_word(address,iopb->flags_opcode,IOPB_MF); address+=2;
    sys_write_long(address,iopb->bufferPtr,IOPB_MF); address+=4;
    sys_write_long(address,iopb->bufferLen,IOPB_MF); address+=4;
    sys_write_word(address,iopb->numBlocks,IOPB_MF); address+=2;
    sys_write_word(address,iopb->unused,IOPB_MF); address+=2;
    sys_write_word(address,iopb->unit,IOPB_MF); address+=2;
    sys_write_word(address,iopb->status,IOPB_MF); address+=2;
    sys_write_long(address,iopb->resDataLen,IOPB_MF); address+=4;
    sys_write_word(address,iopb->status1,IOPB_MF); address+=2;
    sys_write_word(address,iopb->status2,IOPB_MF); address+=2;
    sys_write_word(address,iopb->status3,IOPB_MF);
}


void cs_cmdName (int cmd, char * name) {
    char flags[255];
    switch (cmd & 0x00ff) {
        case CS_CMD_INIT      : strcpy(name,"INIT"); break;
        case CS_CMD_SKIP      : strcpy(name,"SKIP"); break;
        case CS_CMD_REWIND    : strcpy(name,"REWIND"); break;
        case CS_CMD_RETENSION : strcpy(name,"RETENSION"); break;
        case CS_CMD_ERASE     : strcpy(name,"ERASE"); break;
        case CS_CMD_STATUS    : strcpy(name,"STATUS"); break;
        case CS_CMD_TEST      : strcpy(name,"TEST"); break;
        case CS_CMD_READ      : strcpy(name,"READ"); break;
        case CS_CMD_WRITE     : strcpy(name,"WRITE"); break;
        case CS_CMD_APPEND    : strcpy(name,"APPEND"); break;
        default : strcpy(name,"unknown");       
    }
    flags[0] = 0;
    if (cmd & CS_CMDF_OPTIONAL) strcat(flags,"+opt ");  /* Enb. optional command */
    if (cmd & CS_CMDF_INT) strcat(flags,"+int ");       /* Enb. intrpts. */
    if (cmd & CS_CMDF_TESTWRITE) strcat(flags,"+testwr "); /* Enb. write on self-test */
    if (cmd & CS_CMDF_STREAM) strcat(flags,"+strm ");   /* Enb. streaming */
    if (cmd & CS_CMDF_FILEMARKS) strcat(flags,"+fm ");  /* Enb. filemarks */
    if (flags[0] != 0) {
        strcat(name,"[ ");
        strcat(name,flags);
        strcat(name,"]");
    }
}

void cs_processContinue(void) {  /* called each n instructions */
    cs_iopb_t iopb;
    unsigned int address,dataAddr;
    char name[255];
    int i,skipped;
    char filename[FILENAME_MAX+1];
    
    if (cs.execCount) {
        cs.execCount--;
        if (cs.execCount == 0) {
            address = cs.IOPB_addr << 1;
            getIOPB(address,&iopb);
            iopb.status2 = 0; iopb.status3 = 0; /* what are these used for ? */
            iopb.status1 = 0;
            cs_cmdName (iopb.flags_opcode,name);
            msgout (MSGC_FUNC,MYSELF,MSG_NONE,"Starting execution of %04x (%s), iopb @%08x, BufSize(Blocks) #%d, numBlock: #%d",
                    iopb.flags_opcode,name,address,(iopb.bufferLen * 2) / CS_BLOCKSIZE,iopb.numBlocks);
            switch (iopb.flags_opcode & 0xff) {
                case CS_CMD_INIT      : /* data buffer contains 2 words:
                                                1st may be vector (00 c0), 2nd unit number (00 00) */
                                        tape_close_file(); /* does init reset file counter ? */
                                        cs.fileNo = 0;
                                        iopb.status = CS_STAT_COMPLETE;
                                        dataAddr = iopb.bufferPtr << 1;
                                        if ((dataAddr == 0) || (iopb.bufferLen != 2)) {
                                            msgout (MSGC_ERR,MYSELF,MSG_NONE,"INIT: bufferPtr invalid (%08x, address:%08x) or buffer length not 2 (%08x), iopb @%08x",iopb.bufferPtr,dataAddr,iopb.bufferLen,address);
                                            iopb.status = CS_STAT_IPBPERR;
                                        } else {
                                            cs.intVector = sys_read_byte(dataAddr+1,1);
                                            msgout (MSGC_FUNC,MYSELF,MSG_NONE,"using int vector %02x",cs.intVector);
                                        }
                                        break;
                case CS_CMD_SKIP      : tape_close_file();
                                        i = iopb.numBlocks; skipped=0;
                                        while((i>0) && (tape_file_exists(cs.fileNo))) { i--; skipped++; cs.fileNo++; }
                                        iopb.status = CS_STAT_COMPLETE;
                                        if (i>0) iopb.status |= CS_STAT_EOD; /* is this the correct flag ? */
                                        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"Skiped %d files (numBlocks: %d, newFileNo, %d)",skipped,iopb.numBlocks,cs.fileNo);
                                        /* report end of data  */
                                        tape_file_name(cs.fileNo,filename);
                                        if (!(file_exists(filename))) {
                                            iopb.status |= CS_STAT_EOD;
                                            iopb.status1 |= CS_STAT1_EORM;
                                        }
                                        iopb.status1 &= ~CS_STAT1_BOM; /* not at beginning of media */
                                        break;
                case CS_CMD_REWIND    : iopb.status = CS_STAT_COMPLETE;
                                        iopb.status1 = CS_STAT1_STAT1BIT | CS_STAT1_BOM;
                                        tape_close_file();
                                        cs.fileNo = 0;
                                        if (cs.isReadonly) {
                                            iopb.status1 |= CS_STAT1_STAT0BYTE;
                                        }
                                        break;
                case CS_CMD_RETENSION : iopb.status = CS_STAT_COMPLETE;
                                        iopb.status1 = CS_STAT1_STAT1BIT | CS_STAT1_BOM;
                                        tape_close_file();
                                        cs.fileNo = 0;
                                        break;
                case CS_CMD_ERASE     : if (!cs.isReadonly) {
                                            tape_close_file();
                                            if (tape_erase()) iopb.status = CS_STAT_COMPLETE;
                                            else {
                                                iopb.status = CS_STAT_TAPERROR;
                                                iopb.status1= CS_STAT1_DATAERR;
                                            }
                                        } else 
                                            iopb.status = CS_STAT_WRPROT; /* TODO: check if this status is correct */
                                        break;
                case CS_CMD_STATUS    : iopb.status = CS_STAT_COMPLETE;
                                        break;
                case CS_CMD_TEST      : iopb.status = CS_STAT_COMPLETE;
                                        break;
                case CS_CMD_READ      : /*if (iopb.bufferLen & 1) { 
                                            iopb.status = CS_STAT_IPBPERR;
                                        } else*/
                                        if ((iopb.bufferLen * 2) % CS_BLOCKSIZE) 
                                            iopb.status = CS_STAT_IPBPERR;
                                        else
                                        if (cs.fileIsOpen) {
                                            /* if we are at the end of a file, skip to the next one */
                                            if (cs.fileRemainingBlocks == 0) {
                                                tape_close_file();
                                                cs.fileNo++;
                                                if (tape_open_fileR(cs.fileNo)) {
                                                    tape_read(&iopb);
                                                } else {
                                                    /* we are at EOT */
                                                    msgout (MSGC_NOTIMP,MYSELF,MSG_NONE,"READ(without skip), moved to next file but file not present, CS_STAT_READEOT");
                                                    iopb.status = CS_STAT_READEOT | CS_STAT_CHAINTERM;
                                                    iopb.status1 = CS_STAT1_EOM;
                                                }
                                            } else
                                                tape_read(&iopb);
                                        } else
                                        if (tape_open_fileR(cs.fileNo)) {
                                            tape_read(&iopb);
                                        } else {
                                            msgout (MSGC_NOTIMP,MYSELF,MSG_NONE,"READ: failed to open #%d, returning CS_STAT_EOD",cs.fileNo+1);
                                            iopb.status = CS_STAT_EOD; /* correct for end of data ? */
                                            iopb.status1 = CS_STAT1_EOM;
                                        }
                                        iopb.status1 &= ~CS_STAT1_BOM; /* not at beginning of media */
                                        break;
                case CS_CMD_WRITE     : if ((iopb.bufferLen * 2) % CS_BLOCKSIZE) 
                                            iopb.status = CS_STAT_IPBPERR;
                                        else
                                        if (!cs.isReadonly) {
                                          if (iopb.flags_opcode & CS_CMDF_FILEMARKS)
                                            msgout (MSGC_NOTIMP,MYSELF,MSG_NONE,"WRITE: dont know how to handle filemark flag");
                                          if ((cs.fileNo == 0) && (!(cs.fileIsOpenForWrite)))      /* when write at start of tape, erase all files */
                                                tape_erase();
                                          if ((cs.fileIsOpenForWrite) || (tape_at_eod(cs.fileNo))) {
                                            if (cs.numBlocksOnTape > CS_TAPESIZE_BKLS(cs.tapesizeMB)) {
                                                iopb.status = CS_STAT_WROTEEOT | CS_STAT_CHAINTERM;
                                                iopb.status1 = CS_STAT1_EORM;
                                            } else
                                                tape_write(&iopb);
                                          } else
                                            iopb.status = CS_STAT_UNERAAPP;
                                        } else
                                            iopb.status = CS_STAT_WRPROT; /* TODO: check if this status is correct */
                                        break;
                case CS_CMD_APPEND    : if ((iopb.bufferLen * 2) % CS_BLOCKSIZE) 
                                            iopb.status = CS_STAT_IPBPERR;
                                        else 
                                        if (!cs.isReadonly) {
                                            tape_close_file();
                                            tape_count_numBlocks();
                                            while(tape_file_exists(cs.fileNo)) cs.fileNo++;
                                            if (cs.fileNo)
                                                if (tape_is_filemark(cs.fileNo-1))
                                                    cs.fileNo--;
                                            if (cs.numBlocksOnTape > CS_TAPESIZE_BKLS(cs.tapesizeMB)) {
                                                iopb.status = CS_STAT_WROTEEOT | CS_STAT_CHAINTERM;
                                                iopb.status1 = CS_STAT1_EORM;
                                            } else
                                                tape_write(&iopb);
                                        } else
                                            iopb.status = CS_STAT_WRPROT; /* TODO: check if this status is correct */
                                        break;
                default               : msgout (MSGC_NOTIMP,MYSELF,MSG_NONE,"");
                                        iopb.status = CS_STAT_IPBPERR;
                                        iopb.status1 = CS_STAT1_ILLCMD;
            }

            /*
            if (!(iopb.status & CS_STAT_COMPLETE)) {
                if (iopb.nextBlockPtr)
                    iopb.status |= CS_STAT_CHAINTERM;
            }
        */
            if (iopb.nextBlockPtr != 0) {
                /* what is nextBlockPtr, ptr or without bit 0 ?? */
                /* TODO: check with mcsfs */
                msgout (MSGC_FUNC,MYSELF,MSG_NONE,"setting IOPB to next in chain (curr IOPB: %08x (address:%08x), next: %08x (address:%08x)",cs.IOPB_addr,cs.IOPB_addr<<1,iopb.nextBlockPtr,iopb.nextBlockPtr<<1);
                cs.IOPB_addr = iopb.nextBlockPtr;
                /* the controller does not automatically execute the next in chain    
                cs.execCount = 2; */
                cs.goReg = CS_GOREG_DEFAULT;

                /* ?? should each completed iopb generate an interrupt if int flag is set ? */
                /* i assume yes, otherwise it would not make sense to have this flag in each iopb */
            } else {
                cs.goReg = CS_GOREG_DEFAULT;
            }
            if (cs.isReadonly) iopb.status1 |= CS_STAT1_WRITEPROT;
            putIOPB(address,&iopb);
            if (iopb.flags_opcode & CS_CMDF_INT) {
                msgout (MSGC_FUNC,MYSELF,MSG_NONE,"Raising interrupt %d, Vector %02x",CS_INTNO,cs.intVector ? cs.intVector : CS_DEF_INTVEC);
                cs.intCount++;
                m68k_pulse_interrupt (CS_INTNO);
            }
            msgoutn (MSGC_FUNC,MYSELF,MSG_NONE,"End of execution, status: %04x, status1:%04x",iopb.status,iopb.status1);

        }
    }
}


unsigned int cs_read_byte(unsigned int address, int flags) {
#ifdef CS_DISABLE
    BUSERROR(flags,address,MSG_READB);
#else
    // exec uses byte reads msgout (MSGC_ERR,MYSELF,MSG_READB,"%08x (cs should be accessed 16 bit)",address);
    return (cs_read_word(address,flags) & 0xff00) >> 8;
#endif
    return 0xff;
}

unsigned int cs_read_word(unsigned int address, int flags) {
#ifdef CS_DISABLE
    BUSERROR(flags,address,MSG_READW);
#else
    unsigned int value;

    switch (address & CS_ADDR_REGMASK) {
        case CS_GO_REG     : value = cs.goReg;
                             msgout (MSGC_INFO,MYSELF,MSG_READW,"%08x (goReg, returning %04x)",address,value);
                             return value;
        case CS_STATUS_REG : cs.statWriteCount = 0;
                             if (cs.statCmd == STAT_CMD_VER) {
                                 value = 0x4133; /* FIRMWARE REVIDION: A3 */
                                 cs.statCmd = 0;
                             } else
                                value = cs.statusReg;
                             msgout (MSGC_INFO,MYSELF,MSG_READW,"%08x (statusReg, returning %04x)",address,value);
                             return value;
        default            : msgout (MSGC_ERR,MYSELF,MSG_READW,"%08x (unknown)",address); 
    }
#endif
	return 0xffff;

}


void cs_write_byte(unsigned int address, unsigned int value, int flags) {
#ifdef CS_DISABLE
    BUSERROR(flags,address,MSG_WRITEB);
#else
    msgout (MSGC_ERR,MYSELF,MSG_WRITEB,"%02x to %08x (cs should be accessed 16 bit)",value,address);
#endif
}

void cs_write_word(unsigned int address, unsigned int value, int flags) {
#ifdef CS_DISABLE
    BUSERROR(flags,address,MSG_WRITEB);
#else
    static int nextIs = 0;
    unsigned int i;

    switch (address & CS_ADDR_REGMASK) {
        case CS_GO_REG     : cs.statWriteCount = 0;
                             cs.goReg = value;
                             msgout (MSGC_INFO,MYSELF,MSG_WRITEW,"%04x to %08x (goReg)",value,address);
                             /* dont know what the contents means, seems to be a write of any value (found 0000,8002) starts the command */
                             cs.goReg = CS_GOREG_BUSY;
                             cs.execCount = 2;
                             return;
        /* upper bit seems to be a command flag 8000 and 8001 were send */
        /* 8000: Reset controller + drive */
        /* 8001: ??? Assume Chain addr reset */
        /* 8002: Reset controller */
        /* 8003: get firmware version, next read of status: upper byte = version (ascii) */
        case CS_STATUS_REG : if (value & 0xC000) {
                                 nextIs = value >> 14;
                                 if (nextIs == 1) {  /* 01 in upper bits=value in lower bits */
                                    i=value; i &= 0x3fff; i = i << 14;
                                    cs.IOPB_addr &= 0x3fff;
                                    cs.IOPB_addr |= i;
                                    cs.firstIOPB_addr = cs.IOPB_addr;
                                    msgout (MSGC_INFO,MYSELF,MSG_NONE," IOPB(H), got %04x, IOPB:%08x IOPB-addr:%08x",value,cs.IOPB_addr,cs.IOPB_addr<<1);   
                                 } else
                                 if (nextIs == 2) {  /* 8000 */
                                     switch (value) {
                                        case 0x8000:    msgout(MSGC_FUNC,MYSELF,MSG_NONE,"Reset controller and drive");
                                                        /* should i reset IOPB ? */
                                                        break;
                                        case 0x8001:    /* seen from BOSS/IX trestore: sent right
                                                           after the IOPB address handover, so it arms
                                                           the chain at its first block, which is what
                                                           the original commented out guess said */
                                                        msgout(MSGC_FUNC,MYSELF,MSG_NONE,"reset chain address to %08x (address:%08x)",cs.firstIOPB_addr,cs.firstIOPB_addr << 1);
                                                        cs.IOPB_addr = cs.firstIOPB_addr;
                                                        break;
                                        case 0x8002:    msgout(MSGC_FUNC,MYSELF,MSG_NONE,"Reset controller");
                                                        /* must not reset IOPB */
                                                        break;
                                        case 0x8003:    msgout(MSGC_FUNC,MYSELF,MSG_NONE,"Get version");
                                                        cs.statCmd = STAT_CMD_VER;  /* next status read = version */
                                                        break;
                                        default:        msgout(MSGC_NOTIMP,MYSELF,MSG_NONE,"command %04x not known",value);
                                     }
                                 } else
                                    msgout(MSGC_NOTIMP,MYSELF,MSG_NONE,"command %04x not known",value);
                             } else {
                                 msgout (MSGC_INFO,MYSELF,MSG_WRITEW,"%04x to %08x (statusReg), nextIs:%d",value,address,nextIs);
                                 //switch (nextIs) {
                                 /* The IOPB address arrives in two halves, each
                                  * carrying 14 bits, and the byte address is the
                                  * assembled value shifted left once. The high half
                                  * above already merges, keeping the low 14 bits and
                                  * replacing the upper ones. This half has to do the
                                  * mirror image of that. It used to simply assign,
                                  * throwing the upper half away, and the comment here
                                  * said as much: "assume this overwrites upper".
                                  *
                                  * The boot loader never noticed because the addresses
                                  * it uses fit in the low half. The BOSS/IX kernel sends
                                  * 4007 then 15db, which should assemble to 0001d5db and
                                  * a byte address of 0003abb6 in kernel data. Dropping
                                  * the upper half gave 000015db and 00002bb6, which is
                                  * inside kernel TEXT, so when the command finished the
                                  * controller wrote its completion status over the
                                  * instruction at 00002bca. That turned a pea (-$10,A6)
                                  * into a move.l D0,D0 followed by an orphaned fff0, and
                                  * the kernel died on the line F trap a few instructions
                                  * later with "illegal instruction trap, pc = 2BCC".
                                  */
                                             cs.IOPB_addr &= ~0x3fffu;
                                             cs.IOPB_addr |= (value & 0x3fff);
                                             cs.firstIOPB_addr = cs.IOPB_addr;
                                             msgout (MSGC_INFO,MYSELF,MSG_NONE," IOPB(L), got %04x, IOPB:%08x IOPB-addr:%08x",value,cs.IOPB_addr,cs.IOPB_addr<<1);
                                             return;
                                    //default: msgout (MSGC_ERR,MYSELF,MSG_WRITEW,"%08x dont know how to handle write of %04x, idx:%d",address,value,nextIs);
                                 //}
                             }
                             break;
        default            : msgout (MSGC_ERR,MYSELF,MSG_WRITEW,"%04x to %08x (unknown)",value,address);
    }
#endif    
 
}


void cs_pulse_reset(void) {
    static int firsttime = 1;
    char directoryS[FILENAME_MAX+1];
    char filemaskS[FILEMASK_MAX];
    int tapesizeMB;

    if (!firsttime) {
        strcpy(directoryS,cs.directory);
        strcpy(filemaskS,cs.filemask);
        tapesizeMB = cs.tapesizeMB;
    }

    memset(&cs,0,sizeof(cs));
    cs.statusReg = CS_STATREG_DEFAULT;
    cs.goReg = CS_GOREG_DEFAULT;

    if (!firsttime) {
        strcpy(cs.directory,directoryS);
        strcpy(cs.filemask,filemaskS);
        cs.tapesizeMB = tapesizeMB;
    } else {
        strcpy(cs.filemask,CS_FILEMASK);
        strcpy(cs.directory,CS_DEFTAPEDIR);
        cs.tapesizeMB = CS_TAPESIZE_MB_DEF;
    }
    firsttime=0;
}


int  cs_irq_ack(int level) {
    if ((cs.intCount) && (level == CS_INTNO)) {
        cs.intCount--;
        /* mcs waits for interrupt but no INIT command was performed */
        if (cs.intVector) return cs.intVector;
        return CS_DEF_INTVEC;
    }
    return M68K_INT_ACK_SPURIOUS;
}

/******************************************************************************
 * Commands
 ******************************************************************************/

void cs_dir (int numArgs, struct args_t *args) {
    char * p;
    char filename[FILENAME_MAX+1];

    if (numArgs < 1) { printf("cs directory: '%s'\n",cs.directory); return; }

    p = stpcpy(filename,args[0].txt);
    p--;
    while (((char *)p > (char *)&filename) && (*p == ' ')) { *p=0; p--; }

    if (!(file_exists(args[0].txt))) { printf("'%s' does not exist\n",filename); return; }
    if (!(is_directory(args[0].txt))) { printf("'%s' is not a directory\n",filename); return; }

    tape_close_file();
    cs.isReadonly = 1;
    cs.fileNo = 0;
    strcat(filename,"/");
    strcpy(cs.directory,filename);
    cs.isReadonly = access(filename,R_OK | W_OK);
    if (!cs.isReadonly) {               /* if dir is writable */
        tape_file_name(0,filename);     /* set readonly if first file is readonly */
        if (file_exists(filename))
            cs.isReadonly = access(filename,R_OK | W_OK);
    }
    tape_count_numBlocks();
}

void cs_showRegs (int numArgs, struct args_t *args) {
    char protected[30];

    protected[0]=0;
    if (cs.isReadonly) strcpy(protected,"(write protected)");
    printf("cs:    unknown1: %04x IOPB address: %06x = %08x\n" \
           "       unknown2: %04x  lastCommand: %04x\n" \
           " statWriteCount: %d       statusReg: %04x\n" \
           "          goReg: %04x       fileNo: #%d\n" \
           "      directory: %s %s\n" \
           "       filemask: %s\n" \
           "RemainingBlocks: #%d, fileIsIpen: %d  currFileName: '%s'" \
           "     int Vector: %02x            fileIsOpenForWrite: %d\n" \
           "numBlocksOnTape: #%d\n",
           cs.unknown1,cs.IOPB_addr,cs.IOPB_addr << 1,cs.unknown2,cs.lastCommand,
           cs.statWriteCount,cs.statusReg,cs.goReg,cs.fileNo,
           cs.directory,protected,cs.filemask,
           cs.fileRemainingBlocks,cs.fileIsOpen,
           cs.currFilename,cs.intVector,
           cs.fileIsOpenForWrite,cs.numBlocksOnTape);
}


void cs_setiopb (int numArgs, struct args_t *args) {
    cs.IOPB_addr = args[0].value;
}

void cs_setiopb2 (int numArgs, struct args_t *args) {
    cs.IOPB_addr = args[0].value << 1;
}

void cs_showIOPB (int numArgs, struct args_t *args) {
    int maxNum = 1;
    cs_iopb_t iopb;
    unsigned int address;
    char name[20];

    if (numArgs > 0) maxNum = args[0].value;

    address = cs.IOPB_addr << 1;
    while (maxNum > 0) {
        if ((address < 32) || (address > MEM_MAX_RAMROM)) {
            printf("IOPB-address %08x not valid\n",address);
            maxNum = 0;
        } else {
            getIOPB(address,&iopb);
            cs_cmdName (iopb.flags_opcode, name);
            printf("\nIOPB @%08x (address>>1: %08x)\n",address,address>>1);
            printf("NEXT BLOCK POINTER     : %08x (address: %08x)\n" \
                   "FLAGS: %02x    OPCODE    : %02x (%s)\n" \
                   "DATA BUFFER POINTER    : %08x (address:%08x)\n" \
                   "DATA BUFFER LENGTH     : %08x (#%d bytes)\n" \
                   "NO. OF FILES OR BLOCKS : %04x\n" \
                   "  (NOT USED)           : %04x\n" \
                   "UNIT NO.               : %04x\n" \
                   "PRIMARY STATUS WORD    : %04x\n" \
                   "RESIDUAL DATA LENGTH   : %08x\n" \
                   "SEC. STATUS WORDS 1,2,3: %04x %04x %04x\n",
                   iopb.nextBlockPtr,iopb.nextBlockPtr<<1,(iopb.flags_opcode & 0xff00) >> 8,iopb.flags_opcode & 0xff,name,
                   iopb.bufferPtr,iopb.bufferPtr<<1,iopb.bufferLen,iopb.bufferLen<<1,iopb.numBlocks,iopb.unused,
                   iopb.unit,iopb.status,iopb.resDataLen,iopb.status1,
                   iopb.status2,iopb.status3);

            maxNum--;
            address = iopb.nextBlockPtr << 1;
        }
    }
}

void cs_status (int numArgs, struct args_t *args) {
    if (numArgs > 0) cs.statusReg = args[0].value;
    printf("cs statusReg: %04x\n",cs.statusReg);
}

void cs_istatus (int numArgs, struct args_t *args) {
    cs_iopb_t iopb;

    unsigned int address = cs.IOPB_addr << 1;
    getIOPB(address,&iopb);
    
    if (numArgs > 0) {
        iopb.status = args[0].value;
        putIOPB(address,&iopb);
    }
    printf("iopb status: %04x\n",iopb.status);
}

void cs_istatus1 (int numArgs, struct args_t *args) {
    cs_iopb_t iopb;

    unsigned int address = cs.IOPB_addr << 1;
    getIOPB(address,&iopb);
    
    if (numArgs > 0) {
        iopb.status1 = args[0].value;
        putIOPB(address,&iopb);
    }
    printf("iopb status1: %04x\n",iopb.status1);
}


void cs_size (int numArgs, struct args_t *args) {
    int newSize;

    newSize = cs.tapesizeMB;
    if (numArgs > 0) {
        newSize = args[0].value;
        if (newSize < CS_TAPESIZE_MB_MIN)
            newSize = CS_TAPESIZE_MB_MIN;
    }
    cs.tapesizeMB = newSize;
    printf("current tape size: %d MB (%d blocks)\n",cs.tapesizeMB,CS_TAPESIZE_BKLS(cs.tapesizeMB));
}


void cs_help (int numArgs, struct args_t *args);

struct cmds_t csCmds[] =
{
    { "directory",	cs_dir,         0,1,0,"set directory for tape files"},
    { "iopb"     ,  cs_showIOPB,    0,1,1,"show iopb, up to the number of given in last param"},
    { "registers",	cs_showRegs,    0,0,0,"show cs registers"},
    { "setiopb",	cs_setiopb,     0,1,1,"set iopb address"},
    { "setiopbw",	cs_setiopb2,    0,1,1,"set iopb word address"},
    { "size"     ,  cs_size,        0,0,1,"show/change tape size (MB), use #xxx as 2nd param"},
    { "status"   ,  cs_status  ,    0,1,1,"change status register to last param"},
    { "istatus"  ,  cs_istatus ,    0,1,1,"change status in current IOPB to last param"},
    { "istat1"   ,  cs_istatus1,    0,1,1,"change status1 in current IOPB to last param"},
	{ "?",			cs_help,        0,0,0,""},
	{ "help",		cs_help,        0,0,0,"show this help"},
    { "",  NULL, 0,0,0,""}
};

void cs_help (int numArgs, struct args_t *args) {
	showHelp ("cs help commands",csCmds,0);
}


int cs_dbgCmd(int numArgs, struct args_t * args) {
	return findAndExecCommand (args[0].txt,csCmds,numArgs-1,&args[1]);
}


int cs_save_state(FILE * f) {
    STATEWRITEVARS("cs_");

    if ((cs.fileIsOpen) || (cs.fileIsOpenForWrite)) {
        printf("cs_save_state: WARNING: state will differ because tape files are open, closing files\n");
        tape_close_file();
    }

    STATEWRITE(id,f);
    STATEWRITELEN(cs,f);
    return 1;
}

int cs_load_state(FILE * f) {
    STATEREADVARS("cs_");

    tape_close_file();

    STATEREADID(f);
    STATEREADLEN(cs,f);
    return 1;
}
