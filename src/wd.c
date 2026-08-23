/***************************************************************************
 *  wd.c
 *
 *  Created: Nov, 22 2011
 *  Changed: Dec, 21 2011
 *  Armin Diehl <ad@ardiehl.de>
 ****************************************************************************/
/*
 * mai 2000/3000 winchester disk (wd) controller definitions
 * see M8158A '5 1/4" Winchester Disk Drive Controller Service Manual'
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "m68k.h"
#include "wd.h"
#include "sim.h"
#include "memory.h"

int installedWds[WD_MAX] = {WD0_INSTALLED,WD1_INSTALLED};


wd_regs_t wdr[WD_MAX];



#define MYSELF MSG_WD

/* stat after select                         c2: 1100 0010
 *                                           cb: 1100 1011
 * stat after command(expected??):           cc: 1100 1100
 * expected after all command bytes received e8: 1110 1000
 *                                           48: 0100 1000
 *                                               |||| ||||- 0: Bus error
 *                                               |||| |||-- 1: Output data register empty
 *                                               |||| ||--- 2: Operation complete
 *                                               |||| |---- 3: Input data register full
 *                                               ||||------ 4: scsi bus in reset state
 *                                               |||------- 5: scsi bus in message phase
 *                                               ||-------- 6: scsi bus busy
 *                                               |--------- 7: scsi bus in command, status or message phase
 *  and fb = 1111 1011, then expected e8
 */


/* return 1 if command transfers data to host */
int cmdTransferToHost (int cmd) {
    switch (cmd) {
        //case SCSI_TESTREADY     : return 1;
        //case SCSI_REZEROUNIT    : return 1;
        case SCSI_REQUESTSENSE  : return 1;
        //case SCSI_FORMAT        : return 1;
        case SCSI_READ          : return 1;
        //case SCSI_WRITE         : return 1;
        //case SCSI_SEEK          : return 1;
        //case SCSI_TRANSLATE     : return 1;
        //case SCSI_WRITEBUF      : return 1;
        case SCSI_READBUFRAM    : return 1;
        //case SCSI_MODESELECT    : return 1;
        //case SCSI_MODESENSE     : return 1;
        //case SCSI_STARTSTOP     : return 1;
        case SCSI_RECDIAG       : return 1;
        //case SCSI_SENDDIAG      : return 1;
        case SCSI_READCAPACITY  : return 1;
        case SCSI_READ2         : return 1;
        case SCSI_WRITE2        : return 1;
        //case SCSI_WRITEVERIFY   : return 1;
        //case SCSI_VERIFY        : return 1;
        case SCSI_SEARCH        : return 1;
    }
    return 0;
}

int cmdTransferToController (int cmd) {
    switch (cmd) {
        //case SCSI_TESTREADY     : return 1;
        //case SCSI_REZEROUNIT    : return 1;
        //case SCSI_REQUESTSENSE  : return 1;
        case SCSI_FORMAT        : return 1;
        //case SCSI_READ          : return 1;
        case SCSI_WRITE         : return 1;
        //case SCSI_SEEK          : return 1;
        //case SCSI_TRANSLATE     : return 1;
        case SCSI_WRITEBUF      : return 1;
        //case SCSI_READBUFRAM    : return 1;
        //case SCSI_MODESELECT    : return 1;
        //case SCSI_MODESENSE     : return 1;
        //case SCSI_STARTSTOP     : return 1;
        //case SCSI_RECDIAG       : return 1;
        case SCSI_SENDDIAG      : return 1;
        //case SCSI_READCAPACITY  : return 1;
        //case SCSI_READ2         : return 1;
        case SCSI_WRITE2        : return 1;
        case SCSI_WRITEVERIFY   : return 1;
        //case SCSI_VERIFY        : return 1;
        //case SCSI_SEARCH        : return 1;
    }
    return 0;
}


int cmdNoDataTransfer (int cmd) {
    if ( (cmdTransferToController(cmd) == 0) && (cmdTransferToHost(cmd)==0) ) return 1;
    return 0;
}



/******************************************************************************
 * disk image backing store and DMA engine
 *
 * The three DMA address registers hold the ones complement of the system
 * address shifted right once, so the real byte address is
 *     ((~dmaAddress) << 1) & 0xffffff
 * and the register is decremented after every word transferred, which walks
 * the real address upwards. The existing DMA loopback test in this file
 * already relies on that, the transfer routines below use the same rule so
 * both agree.
 ******************************************************************************/

static UINT32 wd_dma_addr (wd_regs_t * wd) {
    return ((~wd->dmaAddress) << 1) & 0x0ffffff;
}

/* controller to system memory, returns 0 and sets BUSERR on a bad address */
static int wd_dma_to_mem (wd_regs_t * wd, UINT8 * buf, int len) {
    int i;
    UINT32 a;

    for (i = 0; i < len; i += 2) {
        a = wd_dma_addr(wd);
        if (!mem_isValidForWrite(a)) {
            msgout (MSGC_ERR,MYSELF,MSG_NONE,"DMA write to invalid address %08x, aborting transfer",a);
            wd->statusReg |= WD_STAT_BUSERR;
            return 0;
        }
        sys_write_word(a,((buf[i] << 8) | buf[i+1]) & 0xffff,1);
        wd->dmaAddress--;
    }
    return 1;
}

/* system memory to controller */
static int wd_dma_from_mem (wd_regs_t * wd, UINT8 * buf, int len) {
    int i;
    UINT32 a;
    UINT16 w;

    for (i = 0; i < len; i += 2) {
        a = wd_dma_addr(wd);
        if (!mem_isValidForRead(a)) {
            msgout (MSGC_ERR,MYSELF,MSG_NONE,"DMA read from invalid address %08x, aborting transfer",a);
            wd->statusReg |= WD_STAT_BUSERR;
            return 0;
        }
        w = sys_read_word(a,1);
        buf[i]   = (w >> 8) & 0xff;
        buf[i+1] = w & 0xff;
        wd->dmaAddress--;
    }
    return 1;
}

static int wd_img_read (wd_regs_t * wd, UINT32 blk, UINT8 * buf, UINT32 nblk) {
    if (!wd->img) return 0;
    if (blk + nblk > wd->imgBlocks) {
        msgout (MSGC_ERR,MYSELF,MSG_NONE,"read past end of image, block %u count %u, image has %u",blk,nblk,wd->imgBlocks);
        return 0;
    }
    if (fseek(wd->img,(long)blk * WD_SECTOR_SIZE,SEEK_SET) != 0) return 0;
    if (fread(buf,WD_SECTOR_SIZE,nblk,wd->img) != nblk) return 0;
    return 1;
}

static int wd_img_write (wd_regs_t * wd, UINT32 blk, UINT8 * buf, UINT32 nblk) {
    if (!wd->img) return 0;
    if (wd->imgReadonly) {
        msgout (MSGC_ERR,MYSELF,MSG_NONE,"write to read only image rejected");
        return 0;
    }
    if (blk + nblk > wd->imgBlocks) {
        msgout (MSGC_ERR,MYSELF,MSG_NONE,"write past end of image, block %u count %u, image has %u",blk,nblk,wd->imgBlocks);
        return 0;
    }
    if (fseek(wd->img,(long)blk * WD_SECTOR_SIZE,SEEK_SET) != 0) return 0;
    if (fwrite(buf,WD_SECTOR_SIZE,nblk,wd->img) != nblk) return 0;
    fflush(wd->img);
    return 1;
}

int wd_attach_image (int unit, const char * name) {
    wd_regs_t * wd;
    long sz;

    if ((unit < 0) || (unit >= WD_MAX)) return 0;
    wd = &wdr[unit];
    if (wd->img) { fclose(wd->img); wd->img = NULL; }
    wd->imgReadonly = 0;
    wd->img = fopen(name,"r+b");
    if (!wd->img) {
        wd->img = fopen(name,"rb");
        if (wd->img) wd->imgReadonly = 1;
    }
    if (!wd->img) {
        printf("wd%d: cannot open '%s'\n",unit,name);
        return 0;
    }
    fseek(wd->img,0,SEEK_END);
    sz = ftell(wd->img);
    if (sz <= 0) { printf("wd%d: '%s' is empty\n",unit,name); fclose(wd->img); wd->img=NULL; return 0; }
    wd->imgBlocks = (UINT32)(sz / WD_SECTOR_SIZE);
    strncpy(wd->imgName,name,sizeof(wd->imgName)-1);
    printf("wd%d: attached '%s', %u blocks (%.1f MB)%s\n",unit,name,wd->imgBlocks,
            (double)wd->imgBlocks * WD_SECTOR_SIZE / 1048576.0,
            wd->imgReadonly ? " read only" : "");
    return 1;
}

int wd_units_ready (void) {
    int i, n = 0;
    for (i = 0; i < WD_MAX; i++) if (wdr[i].installed && wdr[i].img) n++;
    return n;
}

/* pull the logical block address and transfer length out of the CDB */
static void wd_cdb_lba (wd_regs_t * wd, UINT32 * lba, UINT32 * count) {
    int class = (wd->scsiBuf[0] >> 5) & 0x07;

    if (class == 0) {   /* six byte command */
        *lba   = (((UINT32)(wd->scsiBuf[1] & 0x1f)) << 16) |
                 (((UINT32)wd->scsiBuf[2]) << 8) |
                   (UINT32)wd->scsiBuf[3];
        *count = wd->scsiBuf[4];
        if (*count == 0) *count = 256;
    } else {            /* ten byte command */
        *lba   = (((UINT32)wd->scsiBuf[2]) << 24) | (((UINT32)wd->scsiBuf[3]) << 16) |
                 (((UINT32)wd->scsiBuf[4]) << 8)  |   (UINT32)wd->scsiBuf[5];
        *count = (((UINT32)wd->scsiBuf[7]) << 8) | (UINT32)wd->scsiBuf[8];
    }
}


/* Operation complete interrupt.
 *
 * The boot loader drives this controller by polling, so the phase ladder used
 * to advance only when the host read a status or message byte, and the
 * completion interrupt was raised at the very end of the message phase. That is
 * fine while somebody is polling and useless once the driver goes to sleep:
 * BOSS/IX enables INTEN, issues one read and blocks, so nothing ever walked the
 * phases and the interrupt was never reached. The controller has to raise it
 * itself as soon as the command is finished and its status is ready.
 *
 * Modelled as a level, asserted when the command completes and negated on the
 * acknowledge cycle, using the request line rather than forcing an exception so
 * the CPU takes it only when its own mask allows.
 */
static void wd_update_irq (wd_regs_t * wd) {
    int assertIt = wd->intPending ? 1 : 0;

    if (assertIt != wd->intAsserted) {
        wd->intAsserted = assertIt;
        m68k_set_int_line (WD_INTNO, assertIt ? ASSERT_LINE : CLEAR_LINE);
    }
}

static void wd_raise_complete (wd_regs_t * wd) {
    if (!(wd->ctlReg2 & WD_CTL_INTEN)) return;
    msgout (MSGC_INFO,MYSELF,MSG_NONE,"operation complete interrupt, vector %02x",wd->intVector);
    wd->intPending = 1;
    wd_update_irq(wd);
}

void processScsiNextPhase (wd_regs_t * wd) {
    int unit = (wd->scsiBuf[1] >> 5) & 0x07;
    int class;
    int cmd, dmaOn, len;
    UINT32 lba = 0, numBlocks = 0, done, chunk;

    if (wd->state == SCSI_S_IDLE) return;

    /* Phase ladder. The status register values below are the ones the BOSS/IX
     * driver actually tests for, measured by watching it poll cc0009:
     *   0x48  SBUSY+INPFULL              data in phase, a data byte is ready
     *   0xcc  SCMD+SBUSY+OPCOMP+INPFULL  status phase, the status byte is ready
     *   0xe8  SCMD+SMSG+SBUSY+INPFULL    message phase, the message byte is ready
     *   0x00                             bus free, controller deselected
     * The driver spins on cmpi.b #$cc at its command completion loop, so a
     * command that transfers its data by DMA has to land on 0xcc directly
     * without passing through the data in phase.
     */
    if (wd->state == SCSI_S_COMPLETE) {
        if (wd->replyBytesLeft) {
            msgout (MSGC_INFO,MYSELF,MSG_NONE,"command %02x entering data in phase, %d bytes",wd->scsiBuf[0] & 0x1f,wd->replyBytesLeft);
            wd->statusReg = 0x48;
            wd->state = SCSI_S_READRESULTS;
        } else {
            msgout (MSGC_INFO,MYSELF,MSG_NONE,"command %02x entering status phase, status %02x",wd->scsiBuf[0] & 0x1f,wd->statusByte);
            wd->replyBuffer[0] = wd->statusByte;
            wd->replyBytePos = 0;
            wd->replyBytesLeft = 1;
            wd->statusReg = 0xcc;
            wd->state = SCSI_S_STATUS;
            wd_raise_complete(wd);
        }
        return;
    } else
    if (wd->state == SCSI_S_READRESULTS) {
        msgout (MSGC_INFO,MYSELF,MSG_NONE,"data in phase done, status phase, status %02x",wd->statusByte);
        wd->replyBuffer[0] = wd->statusByte;
        wd->replyBytePos = 0;
        wd->replyBytesLeft = 1;
        wd->statusReg = 0xcc;
        wd->state = SCSI_S_STATUS;
        wd_raise_complete(wd);
        return;
    } else
    if (wd->state == SCSI_S_STATUS) {
        msgout (MSGC_INFO,MYSELF,MSG_NONE,"status byte was read, message phase");
        wd->replyBytesLeft = 0;
        wd->readInpReg = 0x00;
        wd->statusReg = 0xe8;
        wd->state = SCSI_S_MESSAGEBYTE;
        return;
    } else
    if (wd->state == SCSI_S_MESSAGEBYTE) {
        msgout (MSGC_INFO,MYSELF,MSG_NONE,"message byte was read, deselect");
        wd->statusReg = 0;
        wd->state = SCSI_S_IDLE;
        wd->selected = 0;
        wd->hostWriteReg = 0;   /* this is returned as status if not selected */
        return;
    } else
    if (wd->state == SCSI_S_ENDOFCOMMAND) {
        wd->statusReg = 0;
        wd->state = SCSI_S_IDLE;
        wd->selected = 0;
        wd->hostWriteReg = 0;
        return;
    }

    wd->replyBytePos=0;
    wd->replyBytesLeft = 1;
    memset(wd->replyBuffer,0,sizeof(wd->replyBuffer));
    class = (wd->scsiBuf[0] >> 5) & 0x07;
    if (class == 0) {
        numBlocks = wd->scsiBuf[4];
    } else
    if (class == 1) {
        numBlocks = ((wd->scsiBuf[7] << 8) & 0xff00) | wd->scsiBuf[8];
    } else {
        msgout (MSGC_ERR|MSGC_BREAK,MYSELF,MSG_NONE,"Invalid class %02x in scsi command, command byte: %02x",class,wd->scsiBuf[0]);
        wd->replyBytesLeft = 1; wd->replyBuffer[0] = 0x02;
        wd->state = SCSI_S_READRESULTS;
        processScsiNextPhase (wd);
        return;
    }

    msgout (MSGC_FUNC,MYSELF,MSG_NONE,"Executing scsi command %02x unit %d blocks %d (%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x)",wd->scsiBuf[0] & 0x1f,unit,numBlocks,
                wd->scsiBuf[0],wd->scsiBuf[1],wd->scsiBuf[2],wd->scsiBuf[3],wd->scsiBuf[4],wd->scsiBuf[5],wd->scsiBuf[6],wd->scsiBuf[7],wd->scsiBuf[8],wd->scsiBuf[9]);
    wd->scsiCheck = 0;
    wd->readInpReg = 0x00;
    wd->replyBytePos = 0;
    wd->replyBytesLeft = 0;             /* only set for byte at a time data in */
    memset(wd->replyBuffer,0,sizeof(wd->replyBuffer));
    class = (wd->scsiBuf[0] >> 5) & 0x07;
    cmd = wd->scsiBuf[0] & 0x1f;
    wd->statusByte = 0x00;              /* 0 good, 2 check condition */
    dmaOn = (wd->ctlReg2 & WD_CTL_SEQEN) ? 1 : 0;
    if (class > 1) {
        msgout (MSGC_ERR,MYSELF,MSG_NONE,"invalid class %d in scsi command byte %02x",class,wd->scsiBuf[0]);
        wd->statusByte = 0x02;
        wd->sense[0] = 0x20;
        wd->state = SCSI_S_COMPLETE;
        wd->stateCounter = WD_PHASE_COUNT;
        return;
    }

    if ((unit > 0) || (!wd->img)) {
        /* only unit 0 exists, and only if an image has been attached */
        if (cmd != SCSI_REQUESTSENSE) {
            msgout (MSGC_INFO,MYSELF,MSG_NONE,"command %02x for unit %d rejected, no drive there",cmd,unit);
            wd->statusByte = 0x02;
            wd->sense[0] = 0x04;        /* drive not ready */
            wd->state = SCSI_S_COMPLETE;
            wd->stateCounter = WD_PHASE_COUNT;
            return;
        }
    }

    switch (cmd) {
        case SCSI_TESTREADY:
        case SCSI_REZEROUNIT:
        case SCSI_SEEK:
        case SCSI_STARTSTOP:
        case SCSI_VERIFY:
                        wd->sense[0] = 0;
                        break;

        case SCSI_REQUESTSENSE:
                        len = wd->scsiBuf[4];
                        if ((len == 0) || (len > 4)) len = 4;
                        if (dmaOn) {
                            memset(wd->dataBuf,0,sizeof(wd->dataBuf));
                            memcpy(wd->dataBuf,wd->sense,4);
                            if (!wd_dma_to_mem(wd,wd->dataBuf,(len+1) & ~1)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x11; break;
                            }
                        } else {
                            memcpy(wd->replyBuffer,wd->sense,4);
                            wd->replyBytesLeft = len;
                        }
                        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"REQUEST SENSE, %d bytes, sense %02x",len,wd->sense[0]);
                        memset(wd->sense,0,sizeof(wd->sense));
                        break;

        case SCSI_READ:
        case SCSI_READ2:
                        wd_cdb_lba(wd,&lba,&numBlocks);
                        if (numBlocks * WD_SECTOR_SIZE > sizeof(wd->dataBuf)) {
                            msgout (MSGC_WARN,MYSELF,MSG_NONE,"read of %u blocks split, buffer holds %u",numBlocks,(UINT32)(sizeof(wd->dataBuf)/WD_SECTOR_SIZE));
                        }
                        done = 0;
                        while (done < numBlocks) {
                            chunk = numBlocks - done;
                            if (chunk > sizeof(wd->dataBuf)/WD_SECTOR_SIZE) chunk = sizeof(wd->dataBuf)/WD_SECTOR_SIZE;
                            if (!wd_img_read(wd,lba+done,wd->dataBuf,chunk)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x14; break;
                            }
                            if (!dmaOn) {
                                msgout (MSGC_ERR,MYSELF,MSG_NONE,"READ without DMA enabled is not supported");
                                wd->statusByte = 0x02; wd->sense[0] = 0x20; break;
                            }
                            if (!wd_dma_to_mem(wd,wd->dataBuf,chunk*WD_SECTOR_SIZE)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x11; break;
                            }
                            done += chunk;
                        }
                        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"READ  lba %u count %u -> memory, status %02x",lba,numBlocks,wd->replyBuffer[0]);
                        break;

        case SCSI_WRITE:
        case SCSI_WRITE2:
        case SCSI_WRITEVERIFY:
                        wd_cdb_lba(wd,&lba,&numBlocks);
                        done = 0;
                        while (done < numBlocks) {
                            chunk = numBlocks - done;
                            if (chunk > sizeof(wd->dataBuf)/WD_SECTOR_SIZE) chunk = sizeof(wd->dataBuf)/WD_SECTOR_SIZE;
                            if (!dmaOn) {
                                msgout (MSGC_ERR,MYSELF,MSG_NONE,"WRITE without DMA enabled is not supported");
                                wd->statusByte = 0x02; wd->sense[0] = 0x20; break;
                            }
                            if (!wd_dma_from_mem(wd,wd->dataBuf,chunk*WD_SECTOR_SIZE)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x11; break;
                            }
                            if (!wd_img_write(wd,lba+done,wd->dataBuf,chunk)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x14; break;
                            }
                            done += chunk;
                        }
                        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"WRITE lba %u count %u <- memory, status %02x",lba,numBlocks,wd->replyBuffer[0]);
                        break;

        case SCSI_SENDDIAG:
                        /* parameter list comes from memory by DMA, length in byte 4 */
                        len = wd->scsiBuf[4];
                        if (len > (int)sizeof(wd->dataBuf)) len = sizeof(wd->dataBuf);
                        if (dmaOn && len) {
                            if (!wd_dma_from_mem(wd,wd->dataBuf,(len+1) & ~1)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x11; break;
                            }
                            msgout (MSGC_FUNC,MYSELF,MSG_NONE,"SEND DIAGNOSTIC, %d parameter bytes: %02x %02x %02x %02x",
                                    len,wd->dataBuf[0],wd->dataBuf[1],wd->dataBuf[2],wd->dataBuf[3]);
                        } else
                            msgout (MSGC_FUNC,MYSELF,MSG_NONE,"SEND DIAGNOSTIC, no parameter transfer (len %d, dma %d)",len,dmaOn);
                        wd->sense[0] = 0;
                        break;

        case SCSI_RECDIAG:
                        /* self test result back to the host, all zero means passed */
                        len = wd->scsiBuf[4];
                        if (len > (int)sizeof(wd->dataBuf)) len = sizeof(wd->dataBuf);
                        memset(wd->dataBuf,0,sizeof(wd->dataBuf));
                        if (dmaOn && len) {
                            if (!wd_dma_to_mem(wd,wd->dataBuf,(len+1) & ~1)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x11; break;
                            }
                        }
                        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"RECEIVE DIAGNOSTIC, %d bytes returned as zero",len);
                        break;

        case SCSI_MODESENSE:
                        /* twelve byte descriptor: 4 header, 8 extent */
                        len = wd->scsiBuf[4];
                        if (len == 0) len = 12;
                        if (len > (int)sizeof(wd->dataBuf)) len = sizeof(wd->dataBuf);
                        memset(wd->dataBuf,0,sizeof(wd->dataBuf));
                        wd->dataBuf[0] = 0;                                 /* reserved */
                        wd->dataBuf[1] = 0;                                 /* medium type */
                        wd->dataBuf[2] = wd->imgReadonly ? 0x80 : 0x00;     /* WP */
                        wd->dataBuf[3] = 8;                                 /* block descriptor length */
                        wd->dataBuf[4] = 0;                                 /* density */
                        wd->dataBuf[5] = (wd->imgBlocks >> 16) & 0xff;
                        wd->dataBuf[6] = (wd->imgBlocks >> 8) & 0xff;
                        wd->dataBuf[7] =  wd->imgBlocks & 0xff;
                        wd->dataBuf[9]  = (WD_SECTOR_SIZE >> 16) & 0xff;
                        wd->dataBuf[10] = (WD_SECTOR_SIZE >> 8) & 0xff;
                        wd->dataBuf[11] =  WD_SECTOR_SIZE & 0xff;
                        if (dmaOn) {
                            if (!wd_dma_to_mem(wd,wd->dataBuf,(len+1) & ~1)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x11; break;
                            }
                        }
                        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"MODE SENSE, %d bytes, %u blocks of %d",len,wd->imgBlocks,WD_SECTOR_SIZE);
                        break;

        case SCSI_MODESELECT:
                        len = wd->scsiBuf[4];
                        if (len > (int)sizeof(wd->dataBuf)) len = sizeof(wd->dataBuf);
                        if (dmaOn && len) {
                            if (!wd_dma_from_mem(wd,wd->dataBuf,(len+1) & ~1)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x11; break;
                            }
                        }
                        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"MODE SELECT, %d bytes accepted and ignored",len);
                        break;

        case SCSI_READCAPACITY:
                        memset(wd->dataBuf,0,sizeof(wd->dataBuf));
                        lba = wd->imgBlocks ? wd->imgBlocks - 1 : 0;
                        wd->dataBuf[0] = (lba >> 24) & 0xff;
                        wd->dataBuf[1] = (lba >> 16) & 0xff;
                        wd->dataBuf[2] = (lba >> 8) & 0xff;
                        wd->dataBuf[3] =  lba & 0xff;
                        wd->dataBuf[6] = (WD_SECTOR_SIZE >> 8) & 0xff;
                        wd->dataBuf[7] =  WD_SECTOR_SIZE & 0xff;
                        if (dmaOn) {
                            if (!wd_dma_to_mem(wd,wd->dataBuf,8)) {
                                wd->statusByte = 0x02; wd->sense[0] = 0x11; break;
                            }
                        }
                        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"READ CAPACITY, last block %u",lba);
                        break;

        case SCSI_FORMAT:
                        msgout (MSGC_FUNC,MYSELF,MSG_NONE,"FORMAT UNIT accepted, image left untouched");
                        wd->sense[0] = 0;
                        break;

        default:        msgout (MSGC_WARN,MYSELF,MSG_NONE,"scsi command %02x not implemented",cmd);
                        wd->statusByte = 0x02;
                        wd->sense[0] = 0x20;    /* invalid command */
    }

    wd->state = SCSI_S_COMPLETE;
    wd->stateCounter = WD_PHASE_COUNT;

}

int processScsiCommand(wd_regs_t * wd);

void processCommand(wd_regs_t * wd) {
    msgout (MSGC_FUNC,MYSELF,MSG_NONE,"end of command %02x, replyBytesLeft:%d",wd->currCommand,wd->replyBytesLeft);
    switch (wd->currCommand) {
        case CMD_RESET     :        //wd->statusReg &= ~(WD_STAT_BUSY);
                                    wd->statusReg = 0; /* expected by test 5 */
                                    break;
        case CMD_SCSIRESET :        wd->statusReg &= ~(WD_STAT_SRESET);
                                    break;
        case CMD_RESET_OUTREGFULL : wd->statusReg |= WD_STAT_OUTEMPTY;
                                    wd->currCommand = CMD_PROCESS_SCSICMD;
                                    wd->cmdCounter = 2;
                                    break;
        case CMD_PROCESS_SCSICMD:   processScsiCommand(wd);
                                    break;
        case CMD_SET_INPFULL:       if (wd->replyBytesLeft) wd->statusReg |= WD_STAT_INPFULL;
                                    else {
                                        if ((wd->state == SCSI_S_READRESULTS) ||
                                            (wd->state == SCSI_S_STATUS) ||
                                            (wd->state == SCSI_S_MESSAGEBYTE) ||
                                            (wd->state == SCSI_S_COMPLETE))
                                                processScsiNextPhase(wd);
                                        else
                                            msgout (MSGC_ERR|MSGC_BREAK,MYSELF,MSG_NONE,"Invalid state %d",wd->state);
                                    }
#if 0
                                    //else wd->statusReg = 0xcc; // test12:0xcc(0x03 sense), test14:0xe8(0x00 testready)
                                    else if (cmdTransferToHost(wd->scsiBuf[0] & 0x1f)) wd->statusReg = 0xCC; // test12: 0x03,sense
                                    else
                                    if (cmdNoDataTransfer(wd->scsiBuf[0] & 0x1f)) wd->statusReg = 0xE8; // test14: 0x00,testready
                                    else msgout (MSGC_ERR|MSGC_BREAK,MYSELF,MSG_NONE,"dont know required status");
#endif
                                    break;

    }
}


void wd_processContinue(void) {  /* called each n instructions */
  if (wdr[0].stateCounter) { wdr[0].stateCounter--; if (wdr[0].stateCounter==0) processScsiNextPhase(&wdr[0]); }
  if (wdr[1].stateCounter) { wdr[1].stateCounter--; if (wdr[1].stateCounter==0) processScsiNextPhase(&wdr[1]); }
  if (wdr[0].cmdCounter) { wdr[0].cmdCounter--; if (wdr[0].cmdCounter==0) processCommand(&wdr[0]); }
  if (wdr[1].cmdCounter) { wdr[1].cmdCounter--; if (wdr[1].cmdCounter==0) processCommand(&wdr[1]); }
}



int processScsiCommand(wd_regs_t * wd) {
    int class,len;

    if (wd->scsiIdx < 6) return 0;
    class = (wd->scsiBuf[0] >> 5) & 0x07;
    switch (class) {
        case 0: len = 6; break;
        case 1: len = 10; break;
        default: msgout (MSGC_ERR,MYSELF,MSG_NONE,"invalid scsi command class %d (cmd=%02x)",class,wd->scsiBuf[0]);
                 return 0;
    }
    if (wd->scsiIdx == len) {
        //printf("--------- got scsi command\n");
        wd->state = SCSI_S_MESSAGE;  /* transfering message to scsi drive */
        wd->statusReg |= 0x20;       /* bus in message phase */
        wd->stateCounter = WD_PHASE_COUNT;
    }
        //wd->statusReg = WD_STAT_SBUSY | WD_STAT_INPFULL;   /* data available */

    return 0;
}

void decodeCtlReg (UINT8 ctlReg, char * tx) {
    char txt[255];
    txt[0]=0; tx[0]=0;
    if (ctlReg & WD_CTL_SRST)    strcat(txt,"SSRST+ ");
    if (ctlReg & WD_CTL_LED)     strcat(txt,"LED+ ");
    if (ctlReg & WD_CTL_INTEN)   strcat(txt,"INTEN+ ");
    if (ctlReg & WD_CTL_SEQEN)	 strcat(txt,"DMA+ ");
    if (ctlReg & WD_CTL_INTEND0) strcat(txt,"INTEND0+ ");
    if (ctlReg & WD_CTL_INTEND1) strcat(txt,"INTEND1+ ");
    if (ctlReg & WD_CTL_INTD0)	 strcat(txt,"INTD0+ ");
    if (ctlReg & WD_CTL_INTD1)   strcat(txt,"INTD1+ ");
    if(txt[0] != 0) {
        txt[strlen(txt)-1] = 0;
        sprintf(tx,"[%s]",txt);
    }
}

void decodeStatusReg(UINT8 statusReg, char * tx) {
    char txt[255];
    txt[0]=0; tx[0]=0;

    if (statusReg & WD_STAT_SCMD)       strcat(txt,"SCMD+ ");
    if (statusReg & WD_STAT_SBUSY)      strcat(txt,"SBUSY+ ");
    if (statusReg & WD_STAT_SMSG)       strcat(txt,"SMSG+ ");
    if (statusReg & WD_STAT_SRESET)     strcat(txt,"SRES+ ");
    if (statusReg & WD_STAT_INPFULL)    strcat(txt,"INPFULL+ ");
    if (statusReg & WD_STAT_OPCOMP)     strcat(txt,"COMPLETE+ ");
    if (statusReg & WD_STAT_OUTEMPTY)   strcat(txt,"OEMPTY+ ");
    if (statusReg & WD_STAT_BUSERR)     strcat(txt,"BUSERR+ ");
    if (statusReg & WD_CTL_SRST)        strcat(txt,"SRST+ ");
    if(txt[0] != 0) {
        txt[strlen(txt)-1] = 0;
        sprintf(tx,"[%s]",txt);
    }
}


unsigned int wd_read_byte(unsigned int address, int flags) {
#ifdef WD_DISABLE
    BUSERROR(flags,address,MSG_READB);
#else
    int regNo;
    wd_regs_t * wd;
    UINT8 value;
    char tx[255];

    wd = NULL;
    if (ADDR_IS_WD0(address)) wd=&wdr[0]; else
    if (ADDR_IS_WD1(address)) wd=&wdr[1];
    regNo = WD_ADDR_TO_REG(address);

    if (wd) {
        if (!(wd->installed)) { BUSERROR(flags,address,MSG_READB); }
        else
        switch (regNo) {             /* not sure if DMA and INTVEC regs are readable -> 01/2021: tested on real hw, they are not, always FF */
            case WD_REG_DMA_HI		: //value = ((wd->dmaAddress >> 16) & 0xff);
                                      //msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x returning DMA H %02x",address,value);
                                      //return value;
                                      msgout (MSGC_ERR,MYSELF,MSG_READB,"%08x attempt ro read write only reg DMA HI",address);
                                      break;
            case WD_REG_DMA_MID		: //value = ((wd->dmaAddress >> 8) & 0xff);
                                      //msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x returning DMA M %02x",address,value);
                                      //return value;
                                      msgout (MSGC_ERR,MYSELF,MSG_READB,"%08x attempt ro read write only reg DMA MID",address);
                                      break;
            case WD_REG_DMA_LOW		: //value = (wd->dmaAddress & 0xff);
                                      //msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x returning DMA L %02x",address,value);
                                      //return value;
                                      msgout (MSGC_ERR,MYSELF,MSG_READB,"%08x attempt ro read write only reg DMA LO",address);
                                      break;
            case WD_REG_INTVEC		: value = wd->intVector;
                                      msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x returning int vector %02x",address,value);
                                      return value;
            case WD_REG_INTVEC2     : /*value = wd->intVectorError;*/
                                      value = wd->intVector;
                                      msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x returning error vector %02x",address,value);
                                      return value;
            case WD_REG_CTL2        : value = wd->ctlReg2;
                                      decodeCtlReg(value,tx);
                                      msgout (MSGC_WARN,MYSELF,MSG_READB,"%08x returning control reg2 %02x %s",address,value,tx);
                                      return value;
            case WD_HOST_WRITE		: msgout (MSGC_ERR,MYSELF,MSG_READB,"%08x attempt ro read write only reg HOST WRITE",address);
                                      break;
            case WD_REG_STAT		: //if (wd->selected) value = wd->statusReg; else value = 0;
									  value = wd->statusReg;
                                      if (wd->ctlReg2 & WD_CTL_SRST) {
                                        //printf("WD_CTL_SRST active, returning 0x10 %02x %02x\n",wd->ctlReg2,wd->ctlReg2 & WD_CTL_SRST);
										value = 0x10 | (wd->statusReg & WD_STAT_BUSERR); /* 0x10 during reset line active, bus err flag expected by test 7 */
									  }


                                      /*if ((wd->scsiCheck) && (value == 0x01)) {
                                          wd->scsiCheck=0;
                                          wd->statusReg=0;
                                      }*/

                                      decodeStatusReg(value,tx);
                                      msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x returning statusReg %02x %s (ctlReg2: %02x, selected:%d)",address,value,tx,wd->ctlReg2,wd->selected);

#if 0
                                      if (complete) {
                                          complete = 0;
                                          wd->statusReg = WD_STAT_SBUSY | WD_STAT_INPFULL; // = 0x48;
                                      }

                                      if (value & WD_STAT_OPCOMP) {         /* after complete */
                                        wd->statusReg &= ~WD_STAT_OPCOMP;   /* clear complete flag */
                                        wd->statusReg &= ~WD_STAT_INPFULL;  /* and input register full */
                                        wd->statusReg |= WD_STAT_OUTEMPTY;
                                        complete = 1;
                                      }
#endif
                                      return value;
            case WD_REG_SELECT      : msgout (MSGC_ERR,MYSELF,MSG_READB,"%08x attempt ro read write only reg SELECT",address);
                                      break;
            case WD_REG_READINP		: if (!(wd->selected)) {
                                            // 0002 moved DMA test read to not selected only
                                            //wd->statusReg &= ~WD_STAT_INPFULL;   /* test 12 */
                                            if (wd->dmaTestCount == 2) {
                                                value = (wd->dmaTestWordRead >> 8) & 0xff;
                                                wd->dmaTestCount--;
                                                msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x returning dmaTest (H) %02x",address,value);
                                            } else
                                            if (wd->dmaTestCount == 1) {
                                                value = wd->dmaTestWordRead & 0xff;
                                                wd->dmaTestCount--;
                                                msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x returning dmaTest (L) %02x",address,value);
                                            } else
                                                value = wd->hostWriteReg; /* loopback */

                                      } else {
                                        wd->scsiIdx=0; /* for test 6 */
                                        if (wd->replyBytesLeft) {
                                           value = wd->replyBuffer[wd->replyBytePos];
                                           wd->replyBytesLeft--;
                                           wd->replyBytePos++;
                                           wd->statusReg &= ~WD_STAT_INPFULL;
                                           wd->cmdCounter = 2;
                                           wd->stateCounter = 0;
                                           wd->currCommand=CMD_SET_INPFULL;
                                           if (wd->replyBytesLeft == 0)
                                                msgout (MSGC_INFO,MYSELF,MSG_NONE,"all reply bytes read");
                                        } else {
                                            value = wd->readInpReg;
                                            if (wd->state == SCSI_S_MESSAGEBYTE) {
                                                wd->statusReg &= ~WD_STAT_INPFULL;
                                                wd->cmdCounter = 2;
                                                wd->stateCounter = 0;
                                                wd->currCommand = CMD_SET_INPFULL;
                                            }
                                        }
                                        msgout (MSGC_INFO,MYSELF,MSG_READB,"%08x returning readInpReg %02x",address,value);
                                      }

                                      return value;
            case WD_REG_CLRBUSERR	: msgout (MSGC_ERR,MYSELF,MSG_READB,"%08x attempt ro read write only reg clrBusErr",address);
                                      break;
            default                 : msgout (MSGC_ERR,MYSELF,MSG_READB,"%08x unknown wd register",address);

        }
    } else BUSERROR(flags,address,MSG_READB);
#endif
	return 0xff;
}

unsigned int wd_read_word(unsigned int address, int flags) {
	return wd_read_byte(address,flags) + ((wd_read_byte(address+1,flags) >> 8) & 0x0ff);
}


void wd_write_byte(unsigned int address, unsigned int value, int flags) {
#ifdef WD_DISABLE
    BUSERROR(flags,address,MSG_WRITEB);
#else
    int regNo;
    wd_regs_t * wd;
    char tx[255];
    UINT32 tmp;
    unsigned int addr;

    wd = NULL;
    if (ADDR_IS_WD0(address)) wd=&wdr[0]; else
    if (ADDR_IS_WD1(address)) wd=&wdr[1];
    regNo = WD_ADDR_TO_REG(address);

    if (wd) {
        if (!(wd->installed)) { BUSERROR(flags,address,MSG_READB); }
        else
        switch (regNo) {
            case WD_REG_DMA_HI		: tmp = value << 16;
                                      wd->dmaAddress &= 0x0ffff;
                                      wd->dmaAddress |= tmp;
                                      msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"%02x to %08x (DMA H), dmaReg:%06x (%06x)",value,address,~wd->dmaAddress,((~wd->dmaAddress) & 0xffffff)<<1);
                                      wd->dmaTestCount = 0;
                                      return;
            case WD_REG_DMA_MID		: tmp = value << 8;
                                      wd->dmaAddress &= 0x0ff00ff;
                                      wd->dmaAddress |= tmp;
                                      msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"%02x to %08x (DMA M), dmaReg:%06x (%06x)",value,address,~wd->dmaAddress,((~wd->dmaAddress) & 0xffffff)<<1);
                                      wd->dmaTestCount = 0;
                                      return;
            case WD_REG_DMA_LOW		: wd->dmaAddress &= 0x0ffff00;
                                      wd->dmaAddress |= value;
                                      msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"%02x to %08x (DMA L), dmaReg:%06x (%06x)",value,address,~wd->dmaAddress,((~wd->dmaAddress) & 0xffffff)<<1);
                                      wd->dmaTestCount = 0;
                                      return;
            case WD_REG_INTVEC      : wd->intVector = value;
                                      msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"%02x to %08x (int vector)",value,address);
#if 0
                                      if (!(wd->selected))
                                        if (!(wd->ctlReg2 & WD_CTL_SEQEN))
                    /* loop-test */       wd->intVectorError = value;  // loopback test, TODO: only in bus free phase and dma disabled
#endif
                                      return;
            case WD_REG_INTVEC2     : /*wd->intVectorError = value;*/
                                      /* can this one be written and what it is used for ? */
                                      msgout (MSGC_INFO|MSGC_BREAK,MYSELF,MSG_WRITEB,"%02x to %08x (int vector error)",value,address);
                                      return;
            /* seems to be that this is the register described under WD_REG_CTL cc0007 */
            case WD_REG_CTL2        : decodeCtlReg(value,tx);
                                      msgout (MSGC_WARN,MYSELF,MSG_WRITEB,"%02x %s to %08x ((R/W Ctrl Reg)",value,tx,address);
                                      if (value & WD_CTL_SRST) {
                                        //wd->statusReg |= WD_STAT_SRESET;  /* (0x10 on real machine) scsi bus reset in progress */
                                        //wd->statusReg = 0x10;     // 0001
                                        wd->cmdCounter = 2;       // 0001
                                        wd->state = SCSI_S_IDLE;  // 0001
                                        wd->replyBytesLeft = 0;   // 0001
                                        wd->currCommand=CMD_RESET;
                                        wd->scsiIdx = 0;
                                        wd->selected = 0;           // 0001 (0001 fails test 6)
                                        //msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"%02x to %08x (R/W Ctrl Reg) statusReg: %02x",value,address,wd->statusReg);
                                      } //else
                                      //if (wd->ctlReg2 & WD_CTL_SRST)  // reset no longer active
                                      //      wd->statusReg = 0xc2;  // will fail test 5
                                      #if 0
                                      if (value == 0)  {
                                          //wd->scsiIdx=0; /* for test 6 */
                                          wd->statusReg &= ~WD_STAT_SBUSY; /* for test 13 */
                                      }
                                      #endif // 0
                                      /* if ints are enabled and the bus error flag is set, generate an interrupt (test 9) */
                                      if ( (value & WD_CTL_INTEN) && (!(wd->ctlReg2 & WD_CTL_INTEN)) && (wd->statusReg & WD_STAT_BUSERR) ) {
                                          msgout (MSGC_INFO,MYSELF,MSG_NONE,"Ints enabled while buserr active, raising interrupt");
                                          //wd->intErrCount++;
                                          wd->intPending = 1;
                                          wd_update_irq(wd);
                                      }
                                      wd->ctlReg2 = value & 0x3f;  // bit 6+7 are readonly
                                      return;
            case WD_HOST_WRITE		: wd->hostWriteReg = value;
                                      msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"%02x to %08x (host write reg) idx:%d, selected:%d, ctlReg2:%02x",value,address,wd->scsiIdx,wd->selected,wd->ctlReg2);
                                      if (wd->selected) {
                                          if (wd->scsiIdx < WD_SCSICMD_MAX) {
                                              wd->scsiBuf[wd->scsiIdx] = value;
                                              wd->scsiIdx++;
                                              /*processScsiCommand(wd); do this after OUTREGFULL is cleared */
                                              /* Test 12 expects WD_STAT_OUTEMPTY to go to 0 */
                                              wd->cmdCounter = 2;
                                              wd->currCommand=CMD_RESET_OUTREGFULL;
                                              wd->statusReg &= ~(WD_STAT_OUTEMPTY);
                                          } else
                                            msgout (MSGC_ERR,MYSELF,MSG_WRITEB,"scsi command buffer overflow");
                                      } else {      /* 0002 assumption: reset deselects controller */
                                          /* DMA-Test: "dummy" IO write (when DMA is enabled and adapter is in Bus-Free-Phase)
                                             reads from memory to CC000B (WD_REG_READINP) */
                                          /* Assume "Bus-Free-Phase" means adapter not selected */
                                        if (wd->ctlReg2 & WD_CTL_SEQEN) { /* only when DMA is on */
                                            addr = ((~wd->dmaAddress) << 1) & 0x0ffffff;
                                            wd->dmaTestWordRead = sys_read_word(addr,1);
                                            wd->dmaTestCount = 2;
                                            wd->dmaAddress--;   /* is inverted word address */
                                            if (mem_isValidForRead (addr)) {
                                                msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"DMA-Test, read %04x from %08x",wd->dmaTestWordRead,addr);
                                            } else {
                                                msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"BusError during DMA-Test, read %04x from %08x",wd->dmaTestWordRead,addr);
                                                wd->statusReg |= WD_STAT_BUSERR;
                                            }
                                         }
                                      }
                                      return;
            case WD_REG_STAT		: msgout (MSGC_ERR,MYSELF,MSG_WRITEB,"attempt to write %02x to %08x (statusReg)",value,address);
                                      break;
            case WD_REG_SELECT      : wd->selected = value;
                                      wd->scsiIdx=0;
                                      wd->statusReg = 0xc2;
                                      //wd->ctlReg2 |= WD_CTL_SRST; /* reset=inactive */
                                      wd->cmdCounter = 0;
                                      msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"%02x to %08x (selected reg) ctlReg2:%02x",value,address,wd->ctlReg2);
                                      return;
            case WD_REG_READINP		: msgout (MSGC_ERR,MYSELF,MSG_WRITEB,"%02x to read only %08x (ReadInp) ignored",value,address);
                                      break;
            case WD_REG_CLRBUSERR	: msgout (MSGC_INFO,MYSELF,MSG_WRITEB,"%02x to %08x (clrBusErr)",value,address);
                                      /* ?? should we generate an interrups ?? */
                                      wd->statusReg &= ~WD_STAT_BUSERR;
                                      break;
            default                 : msgout (MSGC_ERR,MYSELF,MSG_WRITEB,"%02x to %08x (unknown wd register)",value,address);

        }
    } else BUSERROR(flags,address,MSG_READB);
#endif
}

void wd_write_word(unsigned int address, unsigned int value, int flags) {
    wd_write_byte(address,value & 0xff,flags);
}


void wd_pulse_reset(void) {
    int i;
    FILE * keepImg[WD_MAX];
    char   keepName[WD_MAX][FILENAME_MAX+1];
    UINT32 keepBlocks[WD_MAX];
    int    keepRo[WD_MAX];

    /* a reset must not throw away an attached disk */
    for (i=0;i<WD_MAX;i++) {
        keepImg[i]    = wdr[i].img;
        keepBlocks[i] = wdr[i].imgBlocks;
        keepRo[i]     = wdr[i].imgReadonly;
        strncpy(keepName[i],wdr[i].imgName,FILENAME_MAX);
        keepName[i][FILENAME_MAX] = 0;
    }
    memset(&wdr,0,sizeof(wdr));
    for (i=0;i<WD_MAX;i++) {
        wdr[i].img         = keepImg[i];
        wdr[i].imgBlocks   = keepBlocks[i];
        wdr[i].imgReadonly = keepRo[i];
        strncpy(wdr[i].imgName,keepName[i],FILENAME_MAX);
    }
    for (i=0;i<WD_MAX;i++) {
        wdr[i].statusReg = 0xC2; /*(1 << WD_STAT_OUTEMPTY) | (1 << WD_STAT_OPCOMP);*/
    }
    if (WD0_INSTALLED) wdr[0].installed = 1;
    if (WD1_INSTALLED) wdr[1].installed = 1;
}


int  wd_irq_ack(int level) {
    int i;

    if (level != WD_INTNO) return M68K_INT_ACK_SPURIOUS;
    for (i = 0; i < WD_MAX; i++) {
        if ((wdr[i].installed) && (wdr[i].intPending)) {
            wdr[i].intPending = 0;
            wd_update_irq(&wdr[i]);
            return wdr[i].intVector;
        }
    }
    return M68K_INT_ACK_SPURIOUS;
}


/******************************************************************************
 * Commands
 ******************************************************************************/

void wd_showRegs (int numArgs, struct args_t *args) {
    int i;
    char tx[255];
    char tx2[255];

    for (i=0; i<WD_MAX;i++) {
        if (wdr[i].installed) {
            decodeStatusReg(wdr[i].statusReg,tx);
            decodeCtlReg(wdr[i].ctlReg2,tx2);
            printf("wd%d    status: %02x %s\n" \
               "        image: %s (%u blocks)\n" \
               "       vector: %02x  DMA-address: %06x = %08x\n" \
               "     selected: %02x  ReadInpReg: %02x    HostWrite: %02x\n" \
               " stateCounter: %02d       state: %02x\n" \
               "      ctlReg2: %02x %s\n",
                i,wdr[i].statusReg,tx,
                wdr[i].img ? wdr[i].imgName : "<none>",wdr[i].imgBlocks,
                wdr[i].intVector,wdr[i].dmaAddress,(~wdr[i].dmaAddress) & 0xffffff,
                wdr[i].selected,wdr[i].readInpReg,wdr[i].hostWriteReg,
                wdr[i].stateCounter,wdr[i].state,
                wdr[i].ctlReg2,tx2);
        }
    }
}

void wd_help (int numArgs, struct args_t *args);

void wd_image (int numArgs, struct args_t *args) {
    int unit = 0;

    if (numArgs < 1) {
        int i;
        for (i=0;i<WD_MAX;i++)
            if (wdr[i].installed)
                printf("wd%d image: %s (%u blocks)\n",i,
                        wdr[i].img ? wdr[i].imgName : "<none>",wdr[i].imgBlocks);
        return;
    }
    if (numArgs > 1) unit = args[1].value;
    wd_attach_image(unit,args[0].txt);
}

struct cmds_t wdCmds[] =
{
    { "image",      wd_image,       0,2,0,"image <file> [unit] - attach a raw 512 byte per block disk image"},
	{ "registers",	wd_showRegs,    0,0,0,"show wd registers"},
	{ "?",			wd_help,        0,0,0,"show this help"},
	{ "help",		wd_help,        0,0,0,"show this help"},
    { "",  NULL, 0,0,0,""}
};

void wd_help (int numArgs, struct args_t *args) {
	showHelp ("wd help commands",wdCmds,0);
}


int wd_dbgCmd(int numArgs, struct args_t * args) {
	return findAndExecCommand (args[0].txt,wdCmds,numArgs-1,&args[1]);
}


int wd_save_state(FILE * f) {
    STATEWRITEVARS("wd_");

    STATEWRITE(id,f);
    printf("wd_save_state not yet implemented\n");
    return 1;
}

int wd_load_state(FILE * f) {
    STATEREADVARS("wd_");

    STATEREADID(f);
    printf("wd_load_state not yet implemented\n");
    return 1;
}
