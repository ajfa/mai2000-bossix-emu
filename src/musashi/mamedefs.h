#ifndef __CPUINTF_H__
#define __CPUINTF_H__

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOT_IN_MAME

#ifndef INLINE
#define INLINE static __inline__
#endif /* INLINE */

#define logerror fatalerror

/* not supported in GCC prior to 4.4.x */
#if ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 4)) || (__GNUC__ > 4)
#define SETJMP_GNUC_PROTECT()   (void)__builtin_return_address(1)
#else
#define SETJMP_GNUC_PROTECT()   do {} while (0)
#endif


#define DECLARE_LEGACY_CPU_DEVICE(name, basename) \
CPU_INIT( name )


typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
#define  U64

typedef UINT32 offs_t;

/* Disassembler constants */
#define DASMFLAG_SUPPORTED		0x80000000	/* are disassembly flags supported? */
#define DASMFLAG_STEP_OUT		0x40000000	/* this instruction should be the end of a step out sequence */
#define DASMFLAG_STEP_OVER		0x20000000	/* this instruction should be stepped over by setting a breakpoint afterwards */
#define DASMFLAG_OVERINSTMASK	0x18000000	/* number of extra instructions to skip when stepping over */
#define DASMFLAG_OVERINSTSHIFT	27			/* bits to shift after masking to get the value */
#define DASMFLAG_LENGTHMASK		0x0000ffff	/* the low 16-bits contain the actual length */


/* Highly useful macro for compile-time knowledge of an array size */
#define ARRAY_LENGTH(x)         (sizeof(x) / sizeof(x[0]))

#define legacy_cpu_device m68ki_cpu_core
/*#define device_t m68ki_cpu_core */
#define device_t void
#define floatx80 int	/* dummy, we do not support fpu */
#define address_space void
#define m68k_memory_interface void *

#define FALSE 0
#define TRUE 1

#ifndef NULL
#define NULL (void *)0
#endif


typedef int (*device_irq_callback)(device_t *device, int irqnum);
/*typedef int (*cpu_irq_callback)(device_t *device, int irqnum);*/


#define get_safe_token(D) (m68ki_cpu_core *)D


/* standard state indexes */
enum
{
        STATE_GENPC = -1,                               /* generic program counter (live) */
        STATE_GENPCBASE = -2,                   /* generic program counter (base of current instruction) */
        STATE_GENSP = -3,                               /* generic stack pointer */
        STATE_GENFLAGS = -4                             /* generic flags */
};

/* I/O line states */
enum line_state
{
        CLEAR_LINE = 0,                         /* clear (a fired or held) line */
        ASSERT_LINE,                            /* assert an interrupt immediately */
        HOLD_LINE,                                      /* hold interrupt line until acknowledged */
        PULSE_LINE                                      /* pulse interrupt line instantaneously (only for NMI, RESET) */
};

/* address spaces */
enum address_spacenum
{
        AS_0,                                                   /* first address space */
        AS_1,                                                   /* second address space */
        AS_2,                                                   /* third address space */
        AS_3,                                                   /* fourth address space */
        ADDRESS_SPACES,                                 /* maximum number of address spaces */

        /* alternate address space names for common use */
        AS_PROGRAM = AS_0,                              /* program address space */
        AS_DATA = AS_1,                                 /* data address space  */
        AS_IO = AS_2                                    /* I/O address space */
};


#define device_config void

#define CPU_INIT_NAME(name)  cpu_init_##name
#define CPU_INIT(name)       void CPU_INIT_NAME(name)(const device_config *device, device_irq_callback irqcallback)
#define CPU_INIT_CALL(name)  CPU_INIT_NAME(name)(device, irqcallback)

#define CPU_TRANSLATE_NAME(name) cpu_translate_##name
#define CPU_TRANSLATE(name)      int CPU_TRANSLATE_NAME(name)(const device_config *device, int space, int intention, offs_t *address)
#define CPU_TRANSLATE_CALL(name) CPU_TRANSLATE_NAME(name)(device, space, intention, address)

#define CPU_EXECUTE_NAME(name)   cpu_execute_##name
#define CPU_EXECUTE(name)        void CPU_EXECUTE_NAME(name)(device_config *device, int cycles)
#define CPU_EXECUTE_CALL(name)   CPU_EXECUTE_NAME(name)(device, cycles)

#define CPU_DISASSEMBLE_NAME(name) cpu_disassemble_##name
#define CPU_DISASSEMBLE(name)      offs_t CPU_DISASSEMBLE_NAME(name)(device_config *device, char *buffer, offs_t pc, const UINT8 *oprom, const UINT8 *opram, int options)
#define CPU_DISASSEMBLE_CALL(name) CPU_DISASSEMBLE_NAME(name)(device, buffer, pc, oprom, opram, options)

#define CPU_RESET_NAME(name)       cpu_reset_##name
#define CPU_RESET(name)            void CPU_RESET_NAME(name)(device_config *device)
#define CPU_RESET_CALL(name)       CPU_RESET_NAME(name)(device)
/*
#define CPU_IMPORT_STATE_NAME(name)   cpu_state_import_##name
#define CPU_IMPORT_STATE(name)        void CPU_IMPORT_STATE_NAME(name)(const device_config *device, void *baseptr, constcpu_state_entry *entry)
#define CPU_IMPORT_STATE_CALL(name)   CPU_IMPORT_STATE_NAME(name)(device, baseptr, entry)

#define CPU_EXPORT_STATE_NAME(name)   cpu_state_export_##name
#define CPU_EXPORT_STATE(name)        void CPU_EXPORT_STATE_NAME(name)(const device_config *device, void *baseptr, constcpu_state_entry *entry)
#define CPU_EXPORT_STATE_CALL(name)   CPU_EXPORT_STATE_NAME(name)(device, baseptr, entry)
*/
#endif