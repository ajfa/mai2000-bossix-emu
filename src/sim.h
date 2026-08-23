#ifndef SIM__HEADER
#define SIM__HEADER

#define HISTORY_FILENAME "./sim.history"

#define SCC_POLL_INSTRUCTIONS	100000

#define MSGC_MAX	6
#define MSGC_ERR 0
#define MSGC_NOTIMP 1
#define MSGC_WARN 2
#define MSGC_INFO 3
#define MSGC_FATAL 4
#define MSGC_FUNC 5
/* adder */
#define MSGC_BREAK 0x80000000

/* bits for source of message */
#define MSG_OTHER	0
#define MSG_CPU		1
#define MSG_CMB		2
#define MSG_NV		3
#define MSG_FW		4
#define MSG_WD		5
#define MSG_SCC		6
#define MSG_CS		7
#define MSG_PIT		8
#define MSG_MEM		9
#define MSG_FD		10



/* functions raised the message */
#define MSG_NONE 0
#define MSG_READB 1
#define MSG_READW 2
#define MSG_READL 3
#define MSG_WRITEB 4
#define MSG_WRITEW 5
#define MSG_WRITEL 6
#define MSG_SAVE 7
#define MSG_LOAD 8

#define MEM_DISABLEBUSERROR 1
/* to avoid bus errors while disassembling */
#define BUSERROR(F,A,PROC) if (!(F & MEM_DISABLEBUSERROR)) { msgout (MSGC_ERR,MYSELF,PROC,"%08x: generating BUSERR",address); sim_pulse_bus_error(); }

void setCtrlCPressed(void);     /* for break from scc.c */
int messageIsEnabled (int msgClass,int msgSource);

int  msgout(unsigned int msgClass,
            unsigned int msgSource,
            unsigned int routine,
            char * msg, ...);

/* same but with a blank line after output */
void msgoutn(unsigned int msgClass,
             unsigned int msgSource,
             unsigned int routine,
             char * msg, ...);


/* if that fails, use #define MSG msgout */
/* #define MSG(CLASS,SRC,ROUTINE,arg...) if (messageIsEnabled (CLASS,SRC)) (msgout(CLASS,SRC,ROUTINE,arg)) */
#define MSG msgout

unsigned int sys_read_byte(unsigned int address, int memFlags);
unsigned int sys_read_word(unsigned int address, int memFlags);
unsigned int sys_read_long (unsigned int address, int memFlags);
void sys_write_byte(unsigned int address, unsigned int value, int memFlags);
void sys_write_word(unsigned int address, unsigned int value, int memFlags);
void sys_write_long(unsigned int address, unsigned int value, int memFlags);



unsigned int read8(unsigned int address);
unsigned int read16(unsigned int address);
unsigned int read32(unsigned int address);
/* Memory access for the disassembler */
unsigned int m68k_read_disassembler_8  (unsigned int address);
unsigned int m68k_read_disassembler_16 (unsigned int address);
unsigned int m68k_read_disassembler_32 (unsigned int address);

void write8(unsigned int address, unsigned int value);
void write16(unsigned int address, unsigned int value);
void write32(unsigned int address, unsigned int value);
void cpu_pulse_reset(void);
void cpu_set_fc(unsigned int fc);
//int  cpu_irq_ack(int level);

extern unsigned int m68ki_fc;
extern unsigned int m68k_instruction_count;
#define WRITE 1
#define READ 0


#define DEFINPUTTERM "\n "
#define EXTINPUTTERM "\n /."


void invalidArgs(void);
char inputHexval (unsigned int * value, const char * exitChars);

#define MAXCMDLEN 10
#define MAXNUMARGS 4
#define MAXARGLEN 255

struct args_t {
	char txt[MAXARGLEN+1];
	int isValue;
	unsigned int value;
  };

struct cmds_t {
	char name[MAXCMDLEN+1];
	void  (*proc)(int numArgs, struct args_t *args);
	int  minNumArgs;
	int  maxNumArgs;
	int  requiresAllArgsHex;
	char * usage;
};

void showHelp (char * caption, struct cmds_t * cmds, int extended);

int findAndExecCommand (	char * cmd,
                 			struct cmds_t *cmds, int numArgs,
                 			struct args_t *args);

void sim_pulse_bus_error (void);

/* for write state to file */

#define STATEWRITE(DATA,F) if (fwrite(&DATA,1,sizeof(DATA),F) != sizeof(DATA)) return 0
#define STATEWRITELEN(DATA,F) len = sizeof(DATA); if (fwrite(&len,1,sizeof(len),F) != sizeof(len)) return 0; \
if (fwrite(&DATA,1,len,F) != len) return 0

#define STATEREADID(F) if (fread(&idRead,sizeof(idRead),1,F) != 1) return 0; \
    if (memcmp(&id,&idRead,sizeof(id)) != 0) return 0

#define STATEREADLEN(DATA,F) if (fread(&len,sizeof(len),1,F) != 1) return 0; \
    if (len != sizeof (DATA)) return 0; \
    if (fread(&DATA,len,1,F) != 1) return 0;

#define STATEWRITEVARS(ID) char id[3] = ID; \
    UINT32 len

#define STATEREADVARS(ID) char id[3] = ID; \
    char idRead[3]; \
    UINT32 len

void fd_setContinueCounter (int countDown);

#endif /* SIM__HEADER */
