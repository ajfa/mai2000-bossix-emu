/***************************************************************************
 *  m68k.c
 *
 *  Created: Dec, 10 2011
 *  Changed: Dec, 21 2011
 *  Armin Diehl <ad@ardiehl.de>
 * 
 * Compatibility routines to use musashi 4.x (from mame) with 3.3 like
 * interfaces
 ****************************************************************************/

#include <stdio.h>
#include "musashi/m68000.h"
#include "musashi/m68kcpu.h"
#include "m68k.h"
#include "sim.h"


m68ki_cpu_core m68k_cpu;


void m68k_set_cpu_type(int cpu_type) {
	switch (cpu_type) {
		case M68K_CPU_TYPE_68000: { cpu_init_m68000 (&m68k_cpu,NULL); break; }
		case M68K_CPU_TYPE_68010: { cpu_init_m68010 (&m68k_cpu,NULL); break; }
		default: { fatalerror("unsupported cpu\n"); }
	}
}

/* Pulse the RESET pin on the CPU.
 * You *MUST* reset the CPU at least once to initialize the emulation
 * You *MUST* call m68k_set_cpu_type() before resetting
 *       the CPU for the first time
 */
void m68k_pulse_reset(void) {
	cpu_reset_m68k (&m68k_cpu);
}

/* execute num_cycles worth of instructions.  returns number of cycles used */
int m68k_execute(int num_cycles) {
	m68k_cpu.remaining_cycles = num_cycles;
	cpu_execute_m68k(&m68k_cpu, num_cycles);
	return num_cycles - m68k_cpu.remaining_cycles;
}

/* Peek at the internals of a CPU context.  This can either be a context
 * retrieved using m68k_get_context() or the currently running context.
 * If context is NULL, the currently running CPU context will be used.
 */
unsigned int m68k_get_reg(void* context, m68k_register_t reg) {
	m68ki_cpu_core * m68k = context;
	if (m68k == NULL) 
		m68k = &m68k_cpu; 
	
	switch (reg) {	        
		case (M68K_REG_D0): return (REG_D(m68k)[0]);
        case (M68K_REG_D1): return (REG_D(m68k)[1]);
        case (M68K_REG_D2): return (REG_D(m68k)[2]);
        case (M68K_REG_D3): return (REG_D(m68k)[3]);
        case (M68K_REG_D4): return (REG_D(m68k)[4]);
        case (M68K_REG_D5): return (REG_D(m68k)[5]);
        case (M68K_REG_D6): return (REG_D(m68k)[6]);
        case (M68K_REG_D7): return (REG_D(m68k)[7]);
        case (M68K_REG_A0): return (REG_A(m68k)[0]);
        case (M68K_REG_A1): return (REG_A(m68k)[1]);
        case (M68K_REG_A2): return (REG_A(m68k)[2]);
        case (M68K_REG_A3): return (REG_A(m68k)[3]);
        case (M68K_REG_A4): return (REG_A(m68k)[4]);
        case (M68K_REG_A5): return (REG_A(m68k)[5]);
        case (M68K_REG_A6): return (REG_A(m68k)[6]);
        case (M68K_REG_A7): return (REG_A(m68k)[7]);
        case (M68K_REG_PC): return (REG_PC(m68k));
        case (M68K_REG_SR): return m68ki_get_sr(m68k);
        case (M68K_REG_SP): return (REG_SP(m68k));
        case (M68K_REG_USP): return (REG_USP(m68k));          
        case (M68K_REG_ISP): return (REG_ISP(m68k));          
        case (M68K_REG_MSP): return (REG_MSP(m68k));
		case M68K_REG_SFC:      return m68k->sfc;
        case M68K_REG_DFC:      return m68k->dfc;
        case M68K_REG_VBR:      return m68k->vbr;
        case M68K_REG_CACR:     return m68k->cacr;
        case M68K_REG_CAAR:     return m68k->caar;
        case M68K_REG_PREF_ADDR:        return m68k->pref_addr;
        case M68K_REG_PREF_DATA:        return m68k->pref_data;
        case M68K_REG_PPC:      return MASK_OUT_ABOVE_32(m68k->ppc);
        case M68K_REG_IR:       return m68k->ir;
        case M68K_REG_CPU_TYPE:
                        switch(m68k->cpu_type)
                        {
                        	case CPU_TYPE_000:      return (unsigned int)M68K_CPU_TYPE_68000;
                        	case CPU_TYPE_010:      return (unsigned int)M68K_CPU_TYPE_68010;
                        	case CPU_TYPE_EC020:	return (unsigned int)M68K_CPU_TYPE_68EC020;
                        	case CPU_TYPE_020:      return (unsigned int)M68K_CPU_TYPE_68020;
                        }
                        return M68K_CPU_TYPE_INVALID;
		default: fatalerror("m68k_get_reg: unsupported reg %d\n",reg);
	}
	return 0;
}


/* Poke values into the internals of the currently running CPU context */
void m68k_set_reg(m68k_register_t reg, unsigned int value) {
       
	m68ki_cpu_core * m68k = &m68k_cpu;
	switch(reg)
    {
		case M68K_REG_D0:       REG_D(m68k)[0] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_D1:       REG_D(m68k)[1] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_D2:       REG_D(m68k)[2] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_D3:       REG_D(m68k)[3] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_D4:       REG_D(m68k)[4] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_D5:       REG_D(m68k)[5] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_D6:       REG_D(m68k)[6] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_D7:       REG_D(m68k)[7] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_A0:       REG_A(m68k)[0] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_A1:       REG_A(m68k)[1] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_A2:       REG_A(m68k)[2] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_A3:       REG_A(m68k)[3] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_A4:       REG_A(m68k)[4] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_A5:       REG_A(m68k)[5] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_A6:       REG_A(m68k)[6] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_A7:       REG_A(m68k)[7] = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_PC:       m68ki_jump(m68k,MASK_OUT_ABOVE_32(value)); return;
        case M68K_REG_SR:       m68ki_set_sr(m68k,value); return;
        case M68K_REG_SP:       REG_SP(m68k) = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_USP:      if(m68k->s_flag)
        							REG_USP(m68k) = MASK_OUT_ABOVE_32(value);
        						else
                                	REG_SP(m68k) = MASK_OUT_ABOVE_32(value);
                                return;
        case M68K_REG_ISP:      if(m68k->s_flag && !m68k->m_flag)
									REG_SP(m68k) = MASK_OUT_ABOVE_32(value);
								else
									REG_ISP(m68k) = MASK_OUT_ABOVE_32(value);
								return;
        case M68K_REG_MSP:      if(m68k->s_flag && m68k->m_flag)
									REG_SP(m68k) = MASK_OUT_ABOVE_32(value);
								else
                                	REG_MSP(m68k) = MASK_OUT_ABOVE_32(value);
                                return;
        case M68K_REG_VBR:      m68k->vbr = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_SFC:      m68k->sfc = value & 7; return;
        case M68K_REG_DFC:      m68k->dfc = value & 7; return;
        case M68K_REG_CACR:     m68k->cacr = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_CAAR:     m68k->caar = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_PPC:      m68k->ppc = MASK_OUT_ABOVE_32(value); return;
        case M68K_REG_IR:       m68k->ir = MASK_OUT_ABOVE_16(value); return;
        case M68K_REG_CPU_TYPE: m68k_set_cpu_type(value); return;
		default : fatalerror("m68k_set_reg: unsupported reg %d\n",reg); 
	}
}

/* Check if an instruction is valid for the specified CPU type */
/*
unsigned int m68k_is_valid_instruction(unsigned int instruction, unsigned int cpu_type) {
}
*/

/* Disassemble 1 instruction using the epecified CPU type at pc.  Stores
 * disassembly in str_buff and returns the size of the instruction in bytes.
 */
/*
unsigned int m68k_disassemble(char* str_buff, unsigned int pc, unsigned int cpu_type) {
	
}
*/

/* ======================================================================== */
/* ============================== CALLBACKS =============================== */
/* ======================================================================== */

void m68k_set_int_ack_callback(int  (*callback)(device_t *device, int int_level)) {
	m68k_cpu.int_ack_callback = callback;
}

void m68k_set_bkpt_ack_callback(void (*callback)(device_t *device, unsigned int data)) {
	m68k_cpu.bkpt_ack_callback = callback;
}

void m68k_set_reset_instr_callback(void  (*callback)(device_t * device)) {
	m68k_cpu.reset_instr_callback = callback;
}

void m68k_set_instr_callback(void  (*callback)(device_t * device, unsigned int pc)) {
	m68k_cpu.instruction_hook = callback;
}

void m68k_pulse_interrupt (int level) {
	m68ki_exception_interrupt(&m68k_cpu,level);
}

/* Assert or negate an interrupt request line. Unlike m68k_pulse_interrupt this
   only raises the request, the CPU takes it when its own interrupt mask allows,
   which is how the real hardware behaves. */
void set_irq_line(m68ki_cpu_core *m68k, int irqline, int state);

void m68k_set_int_line (int level, int state) {
	set_irq_line(&m68k_cpu,level,state);
}


int cpu_save_state(FILE * f)  {
    STATEWRITEVARS("cpu");

    STATEWRITE(id,f);
    
    STATEWRITELEN(m68k_cpu.cpu_type,f);     /* CPU Type: 68000, 68008, 68010, 68EC020, 68020, 68EC030, 68030, 68EC040, or 68040 */
	STATEWRITELEN(m68k_cpu.dasm_type,f);	 /* disassembly type */
	STATEWRITELEN(m68k_cpu.dar,f);      /* Data and Address Registers */
	STATEWRITELEN(m68k_cpu.ppc,f);		   /* Previous program counter */

	STATEWRITELEN(m68k_cpu.pc,f);           /* Program Counter */
	STATEWRITELEN(m68k_cpu.sp,f);        /* User, Interrupt, and Master Stack Pointers */
	STATEWRITELEN(m68k_cpu.vbr,f);          /* Vector Base Register (m68010+) */
	STATEWRITELEN(m68k_cpu.sfc,f);          /* Source Function Code Register (m68010+) */
	STATEWRITELEN(m68k_cpu.dfc,f);          /* Destination Function Code Register (m68010+) */
	STATEWRITELEN(m68k_cpu.cacr,f);         /* Cache Control Register (m68020, unemulated) */
	STATEWRITELEN(m68k_cpu.caar,f);         /* Cache Address Register (m68020, unemulated) */
	STATEWRITELEN(m68k_cpu.ir,f);           /* Instruction Register */
	//floatx80 fpr[8];     /* FPU Data Register (m68030/040) */
	//UINT32 fpiar;        /* FPU Instruction Address Register (m68040) */
	//UINT32 fpsr;         /* FPU Status Register (m68040) */
	//UINT32 fpcr;         /* FPU Control Register (m68040) */
	STATEWRITELEN(m68k_cpu.t1_flag,f);      /* Trace 1 */
	STATEWRITELEN(m68k_cpu.t0_flag,f);      /* Trace 0 */
	STATEWRITELEN(m68k_cpu.s_flag,f);       /* Supervisor */
	STATEWRITELEN(m68k_cpu.m_flag,f);       /* Master/Interrupt state */
	STATEWRITELEN(m68k_cpu.x_flag,f);       /* Extend */
	STATEWRITELEN(m68k_cpu.n_flag,f);       /* Negative */
	STATEWRITELEN(m68k_cpu.not_z_flag,f);   /* Zero, inverted for speedups */
	STATEWRITELEN(m68k_cpu.v_flag,f);       /* Overflow */
	STATEWRITELEN(m68k_cpu.c_flag,f);       /* Carry */
	STATEWRITELEN(m68k_cpu.int_mask,f);     /* I0-I2 */
	STATEWRITELEN(m68k_cpu.int_level,f);    /* State of interrupt pins IPL0-IPL2 -- ASG: changed from ints_pending */
	STATEWRITELEN(m68k_cpu.stopped,f);      /* Stopped state */
	STATEWRITELEN(m68k_cpu.pref_addr,f);    /* Last prefetch address */
	STATEWRITELEN(m68k_cpu.pref_data,f);    /* Data in the prefetch queue */
	STATEWRITELEN(m68k_cpu.sr_mask,f);      /* Implemented status register bits */
	STATEWRITELEN(m68k_cpu.instr_mode,f);   /* Stores whether we are in instruction mode or group 0/1 exception mode */
	STATEWRITELEN(m68k_cpu.run_mode,f);     /* Stores whether we are processing a reset, bus error, address error, or something else */
	//int    has_pmmu;     /* Indicates if a PMMU available (yes on 030, 040, no on EC030) */
	//int    has_hmmu;     /* Indicates if an Apple HMMU is available in place of the 68851 (020 only) */
	//int    pmmu_enabled; /* Indicates if the PMMU is enabled */
	//int    hmmu_enabled; /* Indicates if the HMMU is enabled */
	//int    has_fpu;      /* Indicates if a FPU is available (yes on 030, 040, may be on 020) */
	//int    fpu_just_reset; /* Indicates the FPU was just reset */
#if 0
	/* Clocks required for instructions / exceptions */
	UINT32 cyc_bcc_notake_b;
	UINT32 cyc_bcc_notake_w;
	UINT32 cyc_dbcc_f_noexp;
	UINT32 cyc_dbcc_f_exp;
	UINT32 cyc_scc_r_true;
	UINT32 cyc_movem_w;
	UINT32 cyc_movem_l;
	UINT32 cyc_shift;
	UINT32 cyc_reset;

	int  initial_cycles;
	int  remaining_cycles;                     /* Number of clocks remaining */
	int  reset_cycles;
#endif
	STATEWRITELEN(m68k_cpu.tracing,f);

	STATEWRITELEN(m68k_cpu.aerr_address,f);
	STATEWRITELEN(m68k_cpu.aerr_write_mode,f);
	STATEWRITELEN(m68k_cpu.aerr_fc,f);

	/* Virtual IRQ lines state */
	STATEWRITELEN(m68k_cpu.virq_state,f);
	STATEWRITELEN(m68k_cpu.nmi_pending,f);
#if 0
	void (**jump_table)(m68ki_cpu_core *m68k);
	const UINT8* cyc_instruction;
	const UINT8* cyc_exception;

	/* Callbacks to host */
	device_irq_callback int_ack_callback;			  /* Interrupt Acknowledge */
	m68k_bkpt_ack_func bkpt_ack_callback;         /* Breakpoint Acknowledge */
	m68k_reset_func reset_instr_callback;         /* Called when a RESET instruction is encountered */
	m68k_cmpild_func cmpild_instr_callback;       /* Called when a CMPI.L #v, Dn instruction is encountered */
	m68k_rte_func rte_instr_callback;             /* Called when a RTE instruction is encountered */
	m68k_tas_func tas_instr_callback;             /* Called when a TAS instruction is encountered, allows / disallows writeback */

	legacy_cpu_device *device;
	address_space *program;
	m68k_memory_interface memory;
	offs_t encrypted_start;
	offs_t encrypted_end;
#endif
	STATEWRITELEN(m68k_cpu.iotemp,f);

	/* ad: bus error handling */
	STATEWRITELEN(m68k_cpu.cpu_buserror_address,f);   /* (first) bus error address */
	STATEWRITELEN(m68k_cpu.cpu_buserror_instraddr,f); /* address of instruction caused bus error */
	STATEWRITELEN(m68k_cpu.cpu_buserror_occurred,f);  /* flag that bus error has occurred from cpu */
	STATEWRITELEN(m68k_cpu.cpu_buserror_on_write,f);  /* write or read access */
	STATEWRITELEN(m68k_cpu.cpu_buserror_byte_transfer,f);  /* is byte transfer */
	STATEWRITELEN(m68k_cpu.cpu_buserror_writeval,f);  /* value to be written */
	STATEWRITELEN(m68k_cpu.cpu_buserror_instrfetch,f); /* 1 if error while fetching instruction */
	STATEWRITELEN(m68k_cpu.cpu_buserror_ir,f);
	STATEWRITELEN(m68k_cpu.cpu_buserror_s_flag,f);     /* Supervisor */

	/* save state data */
	STATEWRITELEN(m68k_cpu.save_sr,f);
	STATEWRITELEN(m68k_cpu.save_stopped,f);
	STATEWRITELEN(m68k_cpu.save_halted,f);
#if 0
	/* PMMU registers */
	UINT32 mmu_crp_aptr, mmu_crp_limit;
	UINT32 mmu_srp_aptr, mmu_srp_limit;
	UINT32 mmu_urp_aptr;	/* 040 only */
	UINT32 mmu_tc;
	UINT16 mmu_sr;
	UINT32 mmu_sr_040;
	UINT32 mmu_atc_tag[MMU_ATC_ENTRIES], mmu_atc_data[MMU_ATC_ENTRIES];
	UINT32 mmu_atc_rr;
	UINT32 mmu_tt0, mmu_tt1;
	UINT32 mmu_itt0, mmu_itt1, mmu_dtt0, mmu_dtt1;
	UINT32 mmu_acr0, mmu_acr1, mmu_acr2, mmu_acr3;

	UINT16 mmu_tmp_sr;      /* temporary hack: status code for ptest and to handle write protection */
	UINT16 mmu_tmp_fc;      /* temporary hack: function code for the mmu (moves) */
	UINT16 mmu_tmp_rw;      /* temporary hack: read/write (1/0) for the mmu */
	UINT32 mmu_tmp_buserror_address;   /* temporary hack: (first) bus error address */
	UINT16 mmu_tmp_buserror_occurred;  /* temporary hack: flag that bus error has occurred from mmu */

	UINT32 ic_address[M68K_IC_SIZE];   /* instruction cache address data */
	UINT16 ic_data[M68K_IC_SIZE];      /* instruction cache content data */

	/* external instruction hook (does not depend on debug mode) */
	
	instruction_hook_t instruction_hook;
#endif
    return 1;
    
}

int cpu_load_state(FILE * f) {
    STATEREADVARS("cpu");

    //if (fread(&idRead,sizeof(idRead),1,f) != 1) return 0;
    //if (memcmp(&id,&idRead,sizeof(id)) != 0) return 0;
    STATEREADID(f);

    STATEREADLEN(m68k_cpu.cpu_type,f);     /* CPU Type: 68000, 68008, 68010, 68EC020, 68020, 68EC030, 68030, 68EC040, or 68040 */
	STATEREADLEN(m68k_cpu.dasm_type,f);	 /* disassembly type */
	STATEREADLEN(m68k_cpu.dar,f);      /* Data and Address Registers */
	STATEREADLEN(m68k_cpu.ppc,f);		   /* Previous program counter */

	STATEREADLEN(m68k_cpu.pc,f);           /* Program Counter */
	STATEREADLEN(m68k_cpu.sp,f);        /* User, Interrupt, and Master Stack Pointers */
	STATEREADLEN(m68k_cpu.vbr,f);          /* Vector Base Register (m68010+) */
	STATEREADLEN(m68k_cpu.sfc,f);          /* Source Function Code Register (m68010+) */
	STATEREADLEN(m68k_cpu.dfc,f);          /* Destination Function Code Register (m68010+) */
	STATEREADLEN(m68k_cpu.cacr,f);         /* Cache Control Register (m68020, unemulated) */
	STATEREADLEN(m68k_cpu.caar,f);         /* Cache Address Register (m68020, unemulated) */
	STATEREADLEN(m68k_cpu.ir,f);           /* Instruction Register */
	//floatx80 fpr[8];     /* FPU Data Register (m68030/040) */
	//UINT32 fpiar;        /* FPU Instruction Address Register (m68040) */
	//UINT32 fpsr;         /* FPU Status Register (m68040) */
	//UINT32 fpcr;         /* FPU Control Register (m68040) */
	STATEREADLEN(m68k_cpu.t1_flag,f);      /* Trace 1 */
	STATEREADLEN(m68k_cpu.t0_flag,f);      /* Trace 0 */
	STATEREADLEN(m68k_cpu.s_flag,f);       /* Supervisor */
	STATEREADLEN(m68k_cpu.m_flag,f);       /* Master/Interrupt state */
	STATEREADLEN(m68k_cpu.x_flag,f);       /* Extend */
	STATEREADLEN(m68k_cpu.n_flag,f);       /* Negative */
	STATEREADLEN(m68k_cpu.not_z_flag,f);   /* Zero, inverted for speedups */
	STATEREADLEN(m68k_cpu.v_flag,f);       /* Overflow */
	STATEREADLEN(m68k_cpu.c_flag,f);       /* Carry */
	STATEREADLEN(m68k_cpu.int_mask,f);     /* I0-I2 */
	STATEREADLEN(m68k_cpu.int_level,f);    /* State of interrupt pins IPL0-IPL2 -- ASG: changed from ints_pending */
	STATEREADLEN(m68k_cpu.stopped,f);      /* Stopped state */
	STATEREADLEN(m68k_cpu.pref_addr,f);    /* Last prefetch address */
	STATEREADLEN(m68k_cpu.pref_data,f);    /* Data in the prefetch queue */
	STATEREADLEN(m68k_cpu.sr_mask,f);      /* Implemented status register bits */
	STATEREADLEN(m68k_cpu.instr_mode,f);   /* Stores whether we are in instruction mode or group 0/1 exception mode */
	STATEREADLEN(m68k_cpu.run_mode,f);     /* Stores whether we are processing a reset, bus error, address error, or something else */
	//int    has_pmmu;     /* Indicates if a PMMU available (yes on 030, 040, no on EC030) */
	//int    has_hmmu;     /* Indicates if an Apple HMMU is available in place of the 68851 (020 only) */
	//int    pmmu_enabled; /* Indicates if the PMMU is enabled */
	//int    hmmu_enabled; /* Indicates if the HMMU is enabled */
	//int    has_fpu;      /* Indicates if a FPU is available (yes on 030, 040, may be on 020) */
	//int    fpu_just_reset; /* Indicates the FPU was just reset */
#if 0
	/* Clocks required for instructions / exceptions */
	UINT32 cyc_bcc_notake_b;
	UINT32 cyc_bcc_notake_w;
	UINT32 cyc_dbcc_f_noexp;
	UINT32 cyc_dbcc_f_exp;
	UINT32 cyc_scc_r_true;
	UINT32 cyc_movem_w;
	UINT32 cyc_movem_l;
	UINT32 cyc_shift;
	UINT32 cyc_reset;

	int  initial_cycles;
	int  remaining_cycles;                     /* Number of clocks remaining */
	int  reset_cycles;
#endif
	STATEREADLEN(m68k_cpu.tracing,f);

	STATEREADLEN(m68k_cpu.aerr_address,f);
	STATEREADLEN(m68k_cpu.aerr_write_mode,f);
	STATEREADLEN(m68k_cpu.aerr_fc,f);

	/* Virtual IRQ lines state */
	STATEREADLEN(m68k_cpu.virq_state,f);
	STATEREADLEN(m68k_cpu.nmi_pending,f);
#if 0
	void (**jump_table)(m68ki_cpu_core *m68k);
	const UINT8* cyc_instruction;
	const UINT8* cyc_exception;

	/* Callbacks to host */
	device_irq_callback int_ack_callback;			  /* Interrupt Acknowledge */
	m68k_bkpt_ack_func bkpt_ack_callback;         /* Breakpoint Acknowledge */
	m68k_reset_func reset_instr_callback;         /* Called when a RESET instruction is encountered */
	m68k_cmpild_func cmpild_instr_callback;       /* Called when a CMPI.L #v, Dn instruction is encountered */
	m68k_rte_func rte_instr_callback;             /* Called when a RTE instruction is encountered */
	m68k_tas_func tas_instr_callback;             /* Called when a TAS instruction is encountered, allows / disallows writeback */

	legacy_cpu_device *device;
	address_space *program;
	m68k_memory_interface memory;
	offs_t encrypted_start;
	offs_t encrypted_end;
#endif
	STATEREADLEN(m68k_cpu.iotemp,f);

	/* ad: bus error handling */
	STATEREADLEN(m68k_cpu.cpu_buserror_address,f);   /* (first) bus error address */
	STATEREADLEN(m68k_cpu.cpu_buserror_instraddr,f); /* address of instruction caused bus error */
	STATEREADLEN(m68k_cpu.cpu_buserror_occurred,f);  /* flag that bus error has occurred from cpu */
	STATEREADLEN(m68k_cpu.cpu_buserror_on_write,f);  /* write or read access */
	STATEREADLEN(m68k_cpu.cpu_buserror_byte_transfer,f);  /* is byte transfer */
	STATEREADLEN(m68k_cpu.cpu_buserror_writeval,f);  /* value to be written */
	STATEREADLEN(m68k_cpu.cpu_buserror_instrfetch,f); /* 1 if error while fetching instruction */
	STATEREADLEN(m68k_cpu.cpu_buserror_ir,f);
	STATEREADLEN(m68k_cpu.cpu_buserror_s_flag,f);     /* Supervisor */

	/* save state data */
	STATEREADLEN(m68k_cpu.save_sr,f);
	STATEREADLEN(m68k_cpu.save_stopped,f);
	STATEREADLEN(m68k_cpu.save_halted,f);
#if 0
	/* PMMU registers */
	UINT32 mmu_crp_aptr, mmu_crp_limit;
	UINT32 mmu_srp_aptr, mmu_srp_limit;
	UINT32 mmu_urp_aptr;	/* 040 only */
	UINT32 mmu_tc;
	UINT16 mmu_sr;
	UINT32 mmu_sr_040;
	UINT32 mmu_atc_tag[MMU_ATC_ENTRIES], mmu_atc_data[MMU_ATC_ENTRIES];
	UINT32 mmu_atc_rr;
	UINT32 mmu_tt0, mmu_tt1;
	UINT32 mmu_itt0, mmu_itt1, mmu_dtt0, mmu_dtt1;
	UINT32 mmu_acr0, mmu_acr1, mmu_acr2, mmu_acr3;

	UINT16 mmu_tmp_sr;      /* temporary hack: status code for ptest and to handle write protection */
	UINT16 mmu_tmp_fc;      /* temporary hack: function code for the mmu (moves) */
	UINT16 mmu_tmp_rw;      /* temporary hack: read/write (1/0) for the mmu */
	UINT32 mmu_tmp_buserror_address;   /* temporary hack: (first) bus error address */
	UINT16 mmu_tmp_buserror_occurred;  /* temporary hack: flag that bus error has occurred from mmu */

	UINT32 ic_address[M68K_IC_SIZE];   /* instruction cache address data */
	UINT16 ic_data[M68K_IC_SIZE];      /* instruction cache content data */

	/* external instruction hook (does not depend on debug mode) */
	
	instruction_hook_t instruction_hook;
#endif
    return 1;
}

