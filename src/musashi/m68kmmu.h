/*
    m68kmmu.h - DUMMY
*/

// MMU status register bit definitions

#define M68K_MMU_SR_BUS_ERROR 0x8000
#define M68K_MMU_SR_SUPERVISOR_ONLY 0x2000
#define M68K_MMU_SR_WRITE_PROTECT 0x0800
#define M68K_MMU_SR_INVALID 0x0400
#define M68K_MMU_SR_MODIFIED 0x0200
#define M68K_MMU_SR_LEVEL_0 0x0000
#define M68K_MMU_SR_LEVEL_1 0x0001
#define M68K_MMU_SR_LEVEL_2 0x0002
#define M68K_MMU_SR_LEVEL_3 0x0003

// MMU translation table descriptor field definitions

#define M68K_MMU_DF_DT         0x0003
#define M68K_MMU_DF_DT0        0x0000
#define M68K_MMU_DF_DT1        0x0001
#define M68K_MMU_DF_DT2        0x0002
#define M68K_MMU_DF_DT3        0x0003
#define M68K_MMU_DF_WP         0x0004
#define M68K_MMU_DF_USED       0x0008
#define M68K_MMU_DF_MODIFIED   0x0010
#define M68K_MMU_DF_CI         0x0040
#define M68K_MMU_DF_SUPERVISOR 0x0100

// MMU ATC Fields

#define M68K_MMU_ATC_BUSERROR  0x08000000
#define M68K_MMU_ATC_CACHE_IN  0x04000000
#define M68K_MMU_ATC_WRITE_PR  0x02000000
#define M68K_MMU_ATC_MODIFIED  0x01000000
#define M68K_MMU_ATC_MASK      0x00ffffff
#define M68K_MMU_ATC_SHIFT     8
#define M68K_MMU_ATC_VALID     0x08000000

// MMU Translation Control register
#define M68K_MMU_TC_SRE        0x02000000

/*
    pmmu_atc_add: adds this address to the ATC
*/
void pmmu_atc_add(m68ki_cpu_core *m68k, UINT32 logical, UINT32 physical, int fc)
{
	fatalerror("m68k: pmmu_atc_add not implemented");
}

/*
    pmmu_atc_flush: flush entire ATC

    7fff0003 001ffd10 80f05750 is what should load
*/
void pmmu_atc_flush(m68ki_cpu_core *m68k)
{
	/*fatalerror("m68k: pmmu_atc_flush: not implemented");*/
}


INLINE UINT32 get_dt2_table_entry(m68ki_cpu_core *m68k, UINT32 tptr, UINT8 ptest)
{
	fatalerror("m68k: get_ft2_table_ebtry: not implemented");
	return 0;
}

INLINE UINT32 get_dt3_table_entry(m68ki_cpu_core *m68k, UINT32 tptr, UINT8 fc, UINT8 ptest)
{
	fatalerror("m68k: get_ft3_table_entry: not implemented");
	return 0;
}


/*
    m68851_mmu_ops: COP 0 MMU opcode handling
*/

void m68881_mmu_ops(m68ki_cpu_core *m68k)
{
	fatalerror("m68k: ,68881_mmu_ops: not implemented");
}


/* Apple HMMU translation is much simpler */
INLINE UINT32 hmmu_translate_addr(m68ki_cpu_core *m68k, UINT32 addr_in)
{
	fatalerror("m68k: hmmu_translate_addr: not implemented");
	return 0;
}
