/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */

#if 0
static const char copyright_notice[] =
"MUSASHI\n"
"Version 4.90 (2011-09-22)\n"
"A portable Motorola M68xxx/CPU32/ColdFire processor emulation engine.\n"
"Copyright Karl Stenerud.  All rights reserved.\n"
"\n"
"This code may be freely used for non-commercial purpooses as long as this\n"
"copyright notice remains unaltered in the source code and any binary files\n"
"containing this code in compiled form.\n"
"\n"
"All other licensing terms must be negotiated with the author\n"
"(Karl Stenerud).\n"
"\n"
"The latest version of this code can be obtained at:\n"
"http://kstenerud.cjb.net or http://mamedev.org\n"
;
#endif


/* ======================================================================== */
/* ================================= NOTES ================================ */
/* ======================================================================== */



/* ======================================================================== */
/* ================================ INCLUDES ============================== */
/* ======================================================================== */

#include "emu.h"
#include "debugger.h"
#include <setjmp.h>
#include "m68kcpu.h"
#include "m68kops.h"
#include "m68kfpu.c"

#include "m68kmmu.h"

extern void m68040_fpu_op0(m68ki_cpu_core *m68k);
extern void m68040_fpu_op1(m68ki_cpu_core *m68k);
extern void m68881_mmu_ops(m68ki_cpu_core *m68k);

/* ======================================================================== */
/* ================================= DATA ================================= */
/* ======================================================================== */

/* Used by shift & rotate instructions */
const UINT8 m68ki_shift_8_table[65] =
{
	0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff
};
const UINT16 m68ki_shift_16_table[65] =
{
	0x0000, 0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
	0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff
};
const UINT32 m68ki_shift_32_table[65] =
{
	0x00000000, 0x80000000, 0xc0000000, 0xe0000000, 0xf0000000, 0xf8000000,
	0xfc000000, 0xfe000000, 0xff000000, 0xff800000, 0xffc00000, 0xffe00000,
	0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000, 0xffff8000,
	0xffffc000, 0xffffe000, 0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00,
	0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0, 0xfffffff8,
	0xfffffffc, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};


/* Number of clock cycles to use for exception processing.
 * I used 4 for any vectors that are undocumented for processing times.
 */
const UINT8 m68ki_exception_cycle_table[7][256] =
{
	{ /* 000 */
		 40, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 34, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 34, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 34, /*  9: Trace                                              */
		  4, /* 10: 1010                                               */
		  4, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 44, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 44, /* 24: Spurious Interrupt                                 */
		 44, /* 25: Level 1 Interrupt Autovector                       */
		 44, /* 26: Level 2 Interrupt Autovector                       */
		 44, /* 27: Level 3 Interrupt Autovector                       */
		 44, /* 28: Level 4 Interrupt Autovector                       */
		 44, /* 29: Level 5 Interrupt Autovector                       */
		 44, /* 30: Level 6 Interrupt Autovector                       */
		 44, /* 31: Level 7 Interrupt Autovector                       */
		 34, /* 32: TRAP #0                                            */
		 34, /* 33: TRAP #1                                            */
		 34, /* 34: TRAP #2                                            */
		 34, /* 35: TRAP #3                                            */
		 34, /* 36: TRAP #4                                            */
		 34, /* 37: TRAP #5                                            */
		 34, /* 38: TRAP #6                                            */
		 34, /* 39: TRAP #7                                            */
		 34, /* 40: TRAP #8                                            */
		 34, /* 41: TRAP #9                                            */
		 34, /* 42: TRAP #10                                           */
		 34, /* 43: TRAP #11                                           */
		 34, /* 44: TRAP #12                                           */
		 34, /* 45: TRAP #13                                           */
		 34, /* 46: TRAP #14                                           */
		 34, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 010 */
		 40, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		126, /*  2: Bus Error                             (unemulated) */
		126, /*  3: Address Error                         (unemulated) */
		 38, /*  4: Illegal Instruction                                */
		 44, /*  5: Divide by Zero                                     */
		 44, /*  6: CHK                                                */
		 34, /*  7: TRAPV                                              */
		 38, /*  8: Privilege Violation                                */
		 38, /*  9: Trace                                              */
		  4, /* 10: 1010                                               */
		  4, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 44, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 46, /* 24: Spurious Interrupt                                 */
		 46, /* 25: Level 1 Interrupt Autovector                       */
		 46, /* 26: Level 2 Interrupt Autovector                       */
		 46, /* 27: Level 3 Interrupt Autovector                       */
		 46, /* 28: Level 4 Interrupt Autovector                       */
		 46, /* 29: Level 5 Interrupt Autovector                       */
		 46, /* 30: Level 6 Interrupt Autovector                       */
		 46, /* 31: Level 7 Interrupt Autovector                       */
		 38, /* 32: TRAP #0                                            */
		 38, /* 33: TRAP #1                                            */
		 38, /* 34: TRAP #2                                            */
		 38, /* 35: TRAP #3                                            */
		 38, /* 36: TRAP #4                                            */
		 38, /* 37: TRAP #5                                            */
		 38, /* 38: TRAP #6                                            */
		 38, /* 39: TRAP #7                                            */
		 38, /* 40: TRAP #8                                            */
		 38, /* 41: TRAP #9                                            */
		 38, /* 42: TRAP #10                                           */
		 38, /* 43: TRAP #11                                           */
		 38, /* 44: TRAP #12                                           */
		 38, /* 45: TRAP #13                                           */
		 38, /* 46: TRAP #14                                           */
		 38, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 020 */
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 030 - not correct */
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 040 */ // TODO: these values are not correct
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 68340 */
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* ColdFire - not correct */
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
};

const UINT8 m68ki_ea_idx_cycle_table[64] =
{
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0, /* ..01.000 no memory indirect, base NULL             */
	 5, /* ..01..01 memory indirect,    base NULL, outer NULL */
	 7, /* ..01..10 memory indirect,    base NULL, outer 16   */
	 7, /* ..01..11 memory indirect,    base NULL, outer 32   */
	 0,  5,  7,  7,  0,  5,  7,  7,  0,  5,  7,  7,
	 2, /* ..10.000 no memory indirect, base 16               */
	 7, /* ..10..01 memory indirect,    base 16,   outer NULL */
	 9, /* ..10..10 memory indirect,    base 16,   outer 16   */
	 9, /* ..10..11 memory indirect,    base 16,   outer 32   */
	 0,  7,  9,  9,  0,  7,  9,  9,  0,  7,  9,  9,
	 6, /* ..11.000 no memory indirect, base 32               */
	11, /* ..11..01 memory indirect,    base 32,   outer NULL */
	13, /* ..11..10 memory indirect,    base 32,   outer 16   */
	13, /* ..11..11 memory indirect,    base 32,   outer 32   */
	 0, 11, 13, 13,  0, 11, 13, 13,  0, 11, 13, 13
};



/***************************************************************************
    CPU STATE DESCRIPTION
***************************************************************************/

#define MASK_ALL				(CPU_TYPE_000 | CPU_TYPE_008 | CPU_TYPE_010 | CPU_TYPE_EC020 | CPU_TYPE_020 | CPU_TYPE_EC030 | CPU_TYPE_030 | CPU_TYPE_EC040 | CPU_TYPE_040 | CPU_TYPE_68340 )
#define MASK_24BIT_SPACE			(CPU_TYPE_000 | CPU_TYPE_008 | CPU_TYPE_010 | CPU_TYPE_EC020)
#define MASK_32BIT_SPACE			(CPU_TYPE_020 | CPU_TYPE_EC030 | CPU_TYPE_030 | CPU_TYPE_EC040 | CPU_TYPE_040 | CPU_TYPE_68340 )
#define MASK_010_OR_LATER			(CPU_TYPE_010 | CPU_TYPE_EC020 | CPU_TYPE_020 | CPU_TYPE_030 | CPU_TYPE_EC030 | CPU_TYPE_040 | CPU_TYPE_EC040 | CPU_TYPE_68340)
#define MASK_020_OR_LATER			(CPU_TYPE_EC020 | CPU_TYPE_020 | CPU_TYPE_EC030 | CPU_TYPE_030 | CPU_TYPE_EC040 | CPU_TYPE_040 | CPU_TYPE_68340)
#define MASK_030_OR_LATER			(CPU_TYPE_030 | CPU_TYPE_EC030 | CPU_TYPE_040 | CPU_TYPE_EC040)
#define MASK_040_OR_LATER			(CPU_TYPE_040 | CPU_TYPE_EC040)



/* ======================================================================== */
/* ================================= API ================================== */
/* ======================================================================== */

void set_irq_line(m68ki_cpu_core *m68k, int irqline, int state)
{
	UINT32 old_level = m68k->int_level;
	UINT32 vstate = m68k->virq_state;
	UINT32 blevel;

	if(state == ASSERT_LINE)
		vstate |= 1 << irqline;
	else
		vstate &= ~(1 << irqline);
	m68k->virq_state = vstate;

	for(blevel = 7; blevel > 0; blevel--)
		if(vstate & (1 << blevel))
			break;

	m68k->int_level = blevel << 8;

	/* A transition from < 7 to 7 always interrupts (NMI) */
	/* Note: Level 7 can also level trigger like a normal IRQ */
	if(old_level != 0x0700 && m68k->int_level == 0x0700)
		m68k->nmi_pending = TRUE;
}

void m68k_presave(m68ki_cpu_core *m68k)
{
	m68k->save_sr = m68ki_get_sr(m68k);
	m68k->save_stopped = (m68k->stopped & STOP_LEVEL_STOP) != 0;
	m68k->save_halted  = (m68k->stopped & STOP_LEVEL_HALT) != 0;
}

void m68k_postload(m68ki_cpu_core *m68k)
{
	m68ki_set_sr_noint_nosp(m68k, m68k->save_sr);
	m68k->stopped = m68k->save_stopped ? STOP_LEVEL_STOP : 0
		        | m68k->save_halted  ? STOP_LEVEL_HALT : 0;
	m68ki_jump(m68k, REG_PC(m68k));
}

void m68k_cause_bus_error(m68ki_cpu_core *m68k)
{
	UINT32 sr;

	sr = m68ki_init_exception(m68k);

	m68k->run_mode = RUN_MODE_BERR_AERR_RESET;

	if (!CPU_TYPE_IS_020_PLUS(m68k->cpu_type))
	{
		/* Note: This is implemented for 68000 only! */
		m68ki_stack_frame_buserr(m68k, sr);
	}
	else if (m68k->mmu_tmp_buserror_address == REG_PPC(m68k))
	{
		m68ki_stack_frame_1010(m68k, sr, EXCEPTION_BUS_ERROR, REG_PPC(m68k), m68k->mmu_tmp_buserror_address);
	}
	else
	{
		m68ki_stack_frame_1011(m68k, sr, EXCEPTION_BUS_ERROR, REG_PPC(m68k), m68k->mmu_tmp_buserror_address);
	}

	m68ki_jump_vector(m68k, EXCEPTION_BUS_ERROR);
}

/* translate logical to physical addresses */
CPU_TRANSLATE( m68k )
{
	return TRUE;
}


/* Execute some instructions until we use up cycles clock cycles */
CPU_EXECUTE( m68k )
{
	m68ki_cpu_core *m68k = get_safe_token(device);

	m68k->initial_cycles = m68k->remaining_cycles;

	/* eat up any reset cycles */
	if (m68k->reset_cycles) {
		int rc = m68k->reset_cycles;
		m68k->reset_cycles = 0;
		m68k->remaining_cycles -= rc;

		if (m68k->remaining_cycles <= 0) return;
	}

	/* See if interrupts came in */
	m68ki_check_interrupts(m68k);

	/* Make sure we're not stopped */
	if(!m68k->stopped)
	{
		/* Return point if we had an address error */
		m68ki_set_address_error_trap(m68k); /* auto-disable (see m68kcpu.h) */

		/* Main loop.  Keep going until we run out of clock cycles */
		while (m68k->remaining_cycles > 0)
		{
			/* Set tracing accodring to T1. (T0 is done inside instruction) */
			m68ki_trace_t1(m68k); /* auto-disable (see m68kcpu.h) */

			/* Call external hook to peek at CPU */
			/* debugger_instruction_hook(device, REG_PC(m68k)); */

			/* call external instruction hook (independent of debug mode) */
			if (m68k->instruction_hook != NULL)
				m68k->instruction_hook(device, REG_PC(m68k));

			/* Record previous program counter */
			REG_PPC(m68k) = REG_PC(m68k);

			m68k->cpu_buserror_instraddr = REG_PC(m68k);
			m68k->cpu_buserror_occurred = 0; /* ad */

			/* save CPU address registers values at start of instruction
			   This is needed for restoring the contents after a nus error */
			int i;
			UINT32 tmp_dar[16];

			for (i = 15; i >= 0; i--)
			{
				tmp_dar[i] = REG_DA(m68k)[i];
			}
			
			if (!m68k->pmmu_enabled)
			{
				m68k->run_mode = RUN_MODE_NORMAL;
				/* Read an instruction and call its handler */
				m68k->ir = m68ki_read_imm_16(m68k);
				/* TODO: handle bus error on instruction read */
				if (m68k->cpu_buserror_occurred) {
					fatalerror("mk68kcpu.h: bus error handling during instruction fetch not yet implemented\n");
					m68k->cpu_buserror_instrfetch = 1;
				} else {
					m68k->jump_table[m68k->ir](m68k);
					m68k->remaining_cycles -= m68k->cyc_instruction[m68k->ir];
				}
			}
			else
			{
				m68k->run_mode = RUN_MODE_NORMAL;
				
				m68k->mmu_tmp_buserror_occurred = 0;
				
				/* Read an instruction and call its handler */
				m68k->ir = m68ki_read_imm_16(m68k);

				if (!m68k->mmu_tmp_buserror_occurred)
				{
					m68k->jump_table[m68k->ir](m68k);
					m68k->remaining_cycles -= m68k->cyc_instruction[m68k->ir];
				}

				if (m68k->mmu_tmp_buserror_occurred)
				{
					UINT32 sr;

					m68k->mmu_tmp_buserror_occurred = 0;

					// restore cpu address registers to value at start of instruction
					for (i = 15; i >= 0; i--)
					{
						if (REG_DA(m68k)[i] != tmp_dar[i])
						{
//                          logerror("PMMU: pc=%08x sp=%08x bus error: fixed %s[%d]: %08x -> %08x\n",
//                                  REG_PPC(m68k), REG_A(m68k)[7], i < 8 ? "D" : "A", i & 7, REG_DA(m68k)[i], tmp_dar[i]);
							REG_DA(m68k)[i] = tmp_dar[i];
						}
					}

					sr = m68ki_init_exception(m68k);

					m68k->run_mode = RUN_MODE_BERR_AERR_RESET;

					if (!CPU_TYPE_IS_020_PLUS(m68k->cpu_type))
					{
						/* Note: This is implemented for 68000 only! */
						m68ki_stack_frame_buserr(m68k, sr);
					}
					else if (m68k->mmu_tmp_buserror_address == REG_PPC(m68k))
					{
						m68ki_stack_frame_1010(m68k, sr, EXCEPTION_BUS_ERROR, REG_PPC(m68k), m68k->mmu_tmp_buserror_address);
					}
					else
					{
						m68ki_stack_frame_1011(m68k, sr, EXCEPTION_BUS_ERROR, REG_PPC(m68k), m68k->mmu_tmp_buserror_address);
					}

					m68ki_jump_vector(m68k, EXCEPTION_BUS_ERROR);

					// TODO:
					/* Use up some clock cycles and undo the instruction's cycles */
					// m68k->remaining_cycles -= m68k->cyc_exception[EXCEPTION_BUS_ERROR] - m68k->cyc_instruction[m68k->ir];
				}
			}

			/* ad: 68010 bus error exception */
			if (m68k->cpu_buserror_occurred)
			{
				/* restore cpu address registers to value at start of instruction */
				for (i = 15; i >= 0; i--)
					REG_DA(m68k)[i] = tmp_dar[i];
				
				m68k->cpu_buserror_s_flag = m68k->s_flag;
				UINT32 sr = m68ki_init_exception(m68k);
				m68k->cpu_buserror_occurred = 0;
				/* 68010 bus error frame */
				/* instruction input buffer contains next instruction */
				m68k->cpu_buserror_ir = readimm16(REG_PC(m68k));
				m68ki_stack_frame_1000(m68k, REG_PC(m68k), sr, EXCEPTION_BUS_ERROR);
				
				m68k->cpu_buserror_instrfetch = 0;
				/* TODO: jump_vecor could raise another exception */
				m68ki_jump_vector(m68k, EXCEPTION_BUS_ERROR);
			}

			/* Trace m68k_exception, if necessary */
			m68ki_exception_if_trace(m68k); /* auto-disable (see m68kcpu.h) */
		}

		/* set previous PC to current PC for the next entry into the loop */
		REG_PPC(m68k) = REG_PC(m68k);
	}
	else if (m68k->remaining_cycles > 0)
		m68k->remaining_cycles = 0;
}

CPU_INIT( m68k )
{
	static UINT32 emulation_initialized = 0;
	m68ki_cpu_core *m68k = get_safe_token(device);

	m68k->cpu_buserror_occurred = 0;
	
	/*m68k->device = device;*/
	/*m68k->program = device->space(AS_PROGRAM);*/
	m68k->int_ack_callback = irqcallback;

	/* disable all MMUs */
	m68k->has_pmmu	       = 0;
	m68k->has_hmmu	       = 0;
	m68k->pmmu_enabled     = 0;
	m68k->hmmu_enabled     = 0;

	/* The first call to this function initializes the opcode handler jump table */
	if(!emulation_initialized)
	{
		m68ki_build_opcode_table();
		emulation_initialized = 1;
	}
}

/* Pulse the RESET line on the CPU */
CPU_RESET( m68k )
{
	m68ki_cpu_core *m68k = get_safe_token(device);

	m68k->cpu_buserror_occurred = 0;

	/* Disable the PMMU/HMMU on reset, if any */
	m68k->pmmu_enabled = 0;
	m68k->hmmu_enabled = 0;

	m68k->mmu_tc = 0;
	m68k->mmu_tt0 = 0;
	m68k->mmu_tt1 = 0;

	/* Clear all stop levels and eat up all remaining cycles */
	m68k->stopped = 0;
	if (m68k->remaining_cycles > 0)
		m68k->remaining_cycles = 0;

	m68k->run_mode = RUN_MODE_BERR_AERR_RESET;

	/* Turn off tracing */
	m68k->t1_flag = m68k->t0_flag = 0;
	m68ki_clear_trace(m68k);
	/* Interrupt mask to level 7 */
	m68k->int_mask = 0x0700;
	m68k->int_level = 0;
	m68k->virq_state = 0;
	/* Reset VBR */
	m68k->vbr = 0;
	/* Go to supervisor mode */
	m68ki_set_sm_flag(m68k, SFLAG_SET | MFLAG_CLEAR);

	/* Invalidate the prefetch queue */
	/* Set to arbitrary number since our first fetch is from 0 */
	m68k->pref_addr = 0x1000;

	/* Read the initial stack pointer and program counter */
	m68ki_jump(m68k, 0);
	REG_SP(m68k) = m68ki_read_imm_32(m68k);
	REG_PC(m68k) = m68ki_read_imm_32(m68k);
	m68ki_jump(m68k, REG_PC(m68k));

	m68k->run_mode = RUN_MODE_NORMAL;

	m68k->reset_cycles = m68k->cyc_exception[EXCEPTION_RESET];

	/* flush the MMU's cache */
	pmmu_atc_flush(m68k);

	if(CPU_TYPE_IS_EC020_PLUS(m68k->cpu_type))
	{
		// clear instruction cache
		m68ki_ic_clear(m68k);
	}

	// disable instruction hook
	m68k->instruction_hook = NULL;
}

CPU_DISASSEMBLE( m68k )
{
	m68ki_cpu_core *m68k = get_safe_token(device);
	return m68k_disassemble_raw(buffer, pc, oprom, opram, m68k->dasm_type);
}




/* global access */

void m68k_set_encrypted_opcode_range(device_t *device, offs_t start, offs_t end)
{
	m68ki_cpu_core *m68k = get_safe_token(device);
	m68k->encrypted_start = start;
	m68k->encrypted_end = end;
}

void m68k_set_hmmu_enable(device_t *device, int enable)
{
	m68ki_cpu_core *m68k = get_safe_token(device);

	m68k->hmmu_enabled = enable;
}


void m68k_set_reset_callback(device_t *device, m68k_reset_func callback)
{
	m68ki_cpu_core *m68k = get_safe_token(device);
	m68k->reset_instr_callback = callback;
}

void m68k_set_cmpild_callback(device_t *device, m68k_cmpild_func callback)
{
	m68ki_cpu_core *m68k = get_safe_token(device);
	m68k->cmpild_instr_callback = callback;
}

void m68k_set_rte_callback(device_t *device, m68k_rte_func callback)
{
	m68ki_cpu_core *m68k = get_safe_token(device);
	m68k->rte_instr_callback = callback;
}

void m68k_set_tas_callback(device_t *device, m68k_tas_func callback)
{
	m68ki_cpu_core *m68k = get_safe_token(device);
	m68k->tas_instr_callback = callback;
}

UINT16 m68k_get_fc(device_t *device)
{
	m68ki_cpu_core *m68k = get_safe_token(device);
	return m68k->mmu_tmp_fc;
}



/****************************************************************************
 * 68000 section
 ****************************************************************************/

CPU_INIT( m68000 )
{
	m68ki_cpu_core *m68k = get_safe_token(device);

	CPU_INIT_CALL(m68k);

	m68k->cpu_type         = CPU_TYPE_000;
	m68k->dasm_type        = M68K_CPU_TYPE_68000;
// hack alert: we use placement new to ensure we are properly initialized
// because we live in the device state which is allocated as bytes
// remove me when we have a real C++ device
	/*new(&m68k->memory) m68k_memory_interface;
	m68k->memory.init16(*m68k->program);*/
	m68k->sr_mask          = 0xa71f; /* T1 -- S  -- -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
	m68k->jump_table       = m68ki_instruction_jump_table[0];
	m68k->cyc_instruction  = m68ki_cycles[0];
	m68k->cyc_exception    = m68ki_exception_cycle_table[0];
	m68k->cyc_bcc_notake_b = -2;
	m68k->cyc_bcc_notake_w = 2;
	m68k->cyc_dbcc_f_noexp = -2;
	m68k->cyc_dbcc_f_exp   = 2;
	m68k->cyc_scc_r_true   = 2;
	m68k->cyc_movem_w      = 2;
	m68k->cyc_movem_l      = 3;
	m68k->cyc_shift        = 1;
	m68k->cyc_reset        = 132;
	m68k->has_pmmu	       = 0;
	m68k->has_hmmu	       = 0;
	m68k->has_fpu	       = 0;
}





/****************************************************************************
 * M68010 section
 ****************************************************************************/

CPU_INIT( m68010 )
{
	m68ki_cpu_core *m68k = get_safe_token(device);

	CPU_INIT_CALL(m68k);

	m68k->cpu_type         = CPU_TYPE_010;
	m68k->dasm_type        = M68K_CPU_TYPE_68010;
// hack alert: we use placement new to ensure we are properly initialized
// because we live in the device state which is allocated as bytes
// remove me when we have a real C++ device
/*	new(&m68k->memory) m68k_memory_interface;*/
/*	m68k->memory.init16(*m68k->program); */
	m68k->sr_mask          = 0xa71f; /* T1 -- S  -- -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
	m68k->jump_table       = m68ki_instruction_jump_table[1];
	m68k->cyc_instruction  = m68ki_cycles[1];
	m68k->cyc_exception    = m68ki_exception_cycle_table[1];
	m68k->cyc_bcc_notake_b = -4;
	m68k->cyc_bcc_notake_w = 0;
	m68k->cyc_dbcc_f_noexp = 0;
	m68k->cyc_dbcc_f_exp   = 6;
	m68k->cyc_scc_r_true   = 0;
	m68k->cyc_movem_w      = 2;
	m68k->cyc_movem_l      = 3;
	m68k->cyc_shift        = 1;
	m68k->cyc_reset        = 130;
	m68k->has_pmmu	       = 0;
	m68k->has_fpu	       = 0;

/*	define_state(device); */
}

