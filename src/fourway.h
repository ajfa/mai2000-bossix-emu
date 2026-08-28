/***************************************************************************
 *  fourway.h
 *
 *  MAI 2000/3000 4-Way controller, four RS-232 ports on a Z80A based
 *  intelligent board. See M8155A '4-Way Controller Service Manual'.
 *
 *  The Z80 is not emulated. Like the cartridge streamer in cs.c, the board is
 *  modelled at the level of its host protocol: the CMB leaves a command block
 *  in main memory and pokes the Instruction Register, the board reads the
 *  block, moves the data packet by DMA and writes a status byte back.
 ****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef FOURWAY_H
#define FOURWAY_H

#include "sim.h"

/* Board base addresses. The header that shipped with this emulator guessed
 * D0A000, D1A000, D2A000, D3A000, which cannot be right because D00000 is the
 * cartridge streamer. The addresses below are the ones BOSS/IX actually probes,
 * measured: it reads a byte from each of d20000, d40000, d60000, d80000,
 * da0000 and dc0000 exactly once and gives up on a bus error. So the boards sit
 * every 0x20000 starting at d20000, which makes the controller ID in the
 * manual's "D(x)A000" notation 2, 4, 6, 8, A and C. */
/* Corrected by Armin Diehl against the real machine: the first 4-Way lives at
 * d40000, not d20000. The kernel does probe d20000 as well, but that slot
 * belongs to something else. With the boards at d4/d6 the boot PROM reports
 * fw [modules= 0,1], the FWAY diagnostic finds boards 0 and 1, and the tty
 * ports 4..7 configure without errors. */
#define FW_BASE_FIRST       0x00d40000
#define FW_BASE_STEP        0x00020000
#define FW_MAX              5
#define FW_ADDR_MASK        0x00ff0000

/* how many boards answer. The configuration record on the reference disk
   declares two 4-Way controllers, so two is the faithful default. */
#define FW_INSTALLED        2

/* Register map, corrected against what the machine actually does. The manual
 * only gives the Instruction Register address. Stage one logging showed the
 * rest: the boot PROM writes ff to base+0 and reads base+2000 and base+c000,
 * and the kernel polls bit 0 of base+2001 exactly where the manual says it
 * watches BUSY, so base+2000 is the status register and base+0 is a control
 * or reset latch. Reads are byte wide at the odd address of each word. */
#define FW_REG_CONTROL      0x0000  /* PROM writes ff, kernel writes 1       */
#define FW_REG_STATUS       0x2000  /* bit 0 is BUSY, polled by the kernel   */
#define FW_REG_INSTR        0xa000  /* manual calls this D(x)A000            */
#define FW_REG_AUX          0xc000  /* read by both PROM and kernel, unknown */
#define FW_REG_MASK         0xe000

/* status register, normal mode, M8155A 3.4.6 */
#define FW_ST_BUSY          0x01    /* board not ready                       */
#define FW_ST_SELFTEST      0x02    /* self test in progress, zero in normal */
#define FW_ST_NOCMDBLOCK    0x04    /* command block address not received    */
#define FW_ST_NOVECTOR      0x08    /* base interrupt vector not received    */
#define FW_ST_DSRA          0x10    /* DSR channel A, negative true          */
#define FW_ST_DSRB          0x20
#define FW_ST_DSRC          0x40
#define FW_ST_DSRD          0x80

/* instruction register, M8155A 3.4.7 */
#define FW_IR_PORTA_READY   0x01
#define FW_IR_PORTB_READY   0x02
#define FW_IR_PORTC_READY   0x04
#define FW_IR_PORTD_READY   0x08
#define FW_IR_PORTSEL_MASK  0x30    /* which port a reset applies to         */
#define FW_IR_PORTSEL_SHIFT 4
#define FW_IR_PORTRESET     0x40
#define FW_IR_INTINHIBIT    0x80

/* command block in host memory, ten bytes per port, M8155A 3.2.1.2 */
#define FW_CB_CMD           0       /* command word                          */
#define FW_CB_COUNT         2       /* data packet byte count                */
#define FW_CB_ADDR_HI       4       /* data packet address, MSB              */
#define FW_CB_ADDR_LO       6       /* data packet address, LSB              */
#define FW_CB_STATUS        8       /* transfer status, board back to CMB    */
#define FW_CB_PORTSIZE      10
#define FW_PORTS            4

/* commands */
#define FW_CMD_CONF         0x01
#define FW_CMD_DT           0x02    /* data transfer                         */
#define FW_CMD_STAT         0x03
#define FW_CMD_LDDEF        0x04    /* load default parameters               */
#define FW_CMD_LDXON        0x05
#define FW_CMD_LDXOFF       0x06
#define FW_CMD_SINGL        0x07    /* single byte transfer                  */
#define FW_CMD_ENXON        0x08
#define FW_CMD_ENFLOW       0x09
#define FW_CMD_EIGHT        0x0A    /* seven or eight bit                    */
/* Command termination, M8155A 3.2.1.4. The board writes one of these into the
   status byte of the command block when it is done. Zero means nothing has
   happened yet, which is why writing zero left the driver waiting forever. */
#define FW_ST_EXECUTED      0x81    /* command has been executed             */
#define FW_ST_NOTUNDERSTOOD 0x83    /* command not understood, or bus error  */

/* Interrupt vector encoding, M8155A 3.2.2.2. The board returns the base vector
   plus four per channel plus the condition, not the base on its own. */
#define FW_VEC_EXTSTATUS    0
#define FW_VEC_RXCHAR       1
#define FW_VEC_SPECIALRX    2
#define FW_VEC_CMDEXECUTED  3
#define FW_CMD_ILLEGAL      0xff    /* returned in the status byte           */

#define ADDR_IS_FW(A)  (fw_decode(A) >= 0)

int  fw_decode (unsigned int address);   /* board index, or -1 */

unsigned int fw_read_byte (unsigned int address, int flags);
unsigned int fw_read_word (unsigned int address, int flags);
void fw_write_byte (unsigned int address, unsigned int value, int flags);
void fw_write_word (unsigned int address, unsigned int value, int flags);
void fw_pulse_reset (void);
int  fw_irq_ack (int level);
int  fw_dbgCmd (int numArgs, struct args_t * args);

#endif
