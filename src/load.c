/* load.c
 * load s-record file
 * ad 08 Dec 2011

   ad 22 Dec 2020: corrected S Record types 9 and 6, was swapped
   ad 10 Jan 2020: try filename + .srec first
 */

#include <stdio.h>
#include <string.h>
#include "m68k.h"
#include "sim.h"

int byteSum,recNum,numBytesLoaded;
unsigned int lowAddress,highAddress;

void skip (FILE *f, long bytes) {
    fseek(f,bytes*2,SEEK_CUR);
}

char nextCh (FILE *f) {
  char c;

  if (fread(&c,1,1,f) != 1) c = 0;
  return c;
}

unsigned int hexNib2b (char c) {
	if ((c >= '0') && (c <= '9'))
    	return (c - '0');
	if ((c >= 'A') && (c <= 'F'))
		return (c - 'A' + 10);
	printf ("Illegal Hex char %c\n",c);
	return 0;
}

unsigned int getbyte (FILE *f) {
    return ((hexNib2b (nextCh(f)) << 4) + hexNib2b (nextCh(f)));
}


int saveData (FILE *f, unsigned int address, int remaining) {
	unsigned int b,c;
	/*printf("%08x ",address);*/
	while (remaining > 0) {
		b = getbyte(f);
		byteSum += b;
		/*printf("%02x ",b);*/
		cpu_write_byte(address, b);
		c = cpu_read_byte(address);
		if (address < lowAddress) lowAddress = address;
		if (address > highAddress) highAddress = address;
		if (c != b)
			printf("memory verify error address %08x, written: %02x, read: %02x\n",address,b,c);
		numBytesLoaded++;
		address++; remaining--;
	}
	byteSum += getbyte(f);	/* checksum */
	if ((byteSum & 0xff) != 0xff) {
		printf("Invalid checksum in record number %d\r\n",recNum);
		return 1;
	}
	return 0;
}

#define EXIT fclose(f);	printf("%s: %d lines processed, %d bytes loaded, %d error(s) (lowest addr: %08x, highest: %08x)\n",fn,recNum,numBytesLoaded,errs,lowAddress,highAddress); return errs
#define CHKSUM if ((byteSum & 0xff) != 0xff) printf("Invalid checksum in record number %d (S%c)\r\n",recNum,cmd)

int loadSrecordFile (char * filename, unsigned int offset, unsigned int *startAddr) {
	FILE* f;
	char c,cmd;
	int len,a1,a2,a3,a4,i;
	unsigned int address;
	int recNum = 0;
	int errs = 0;
	char fn[512];
	char s[512];
	int isDiag = 0;

	strcpy(fn,filename); strcat(fn,".srec");

	*startAddr = 0xffffffff;
	numBytesLoaded = 0;
	lowAddress = 0xffffffff;
	highAddress = 0;

	if((f = fopen(fn, "rb")) == NULL) {
		strcpy(fn,filename);
		if((f = fopen(fn, "rb")) == NULL) {
			printf ("unable to open %s\n",filename);
			return 1;
		}
	}
	c = nextCh(f);
	if (c == 'H') {		/* may be file from diag HDR1 */
		len = fread(&s,1,3,f);
		if (len != 3) { errs++; EXIT; }
		/* skip header */
		fseek(f,0x200-4,SEEK_CUR);
		c = nextCh(f);
		isDiag = 1;
	}
	while ((c == 13) || (c == 10)) c = nextCh(f);
	while (c == 'S') {
		cmd = nextCh(f);
		while ((cmd == 13) || (cmd == 10)) cmd = nextCh(f);
		len = getbyte(f);
		byteSum = len;
		recNum++;
		/* printf("S%c len: %02x RecNum: %d\n",cmd,len,recNum); */
		switch (cmd) {
			case '0' : {		/* header: 20 modulname, 2 version, 2 revision, 0-36 description */
    			skip(f,len);
				break;
			}
			case '1' : {		/* data, 2 byte address */
				a1 = getbyte(f); byteSum+=a1;
				a2 = getbyte(f); byteSum+=a2;
				address = (a1 << 8) + a2;
				errs += saveData(f,address+offset,len-3);
				break;
			}
			case '2' : {        /* data, 3 byte address */
				a1 = getbyte(f); byteSum+=a1;
				a2 = getbyte(f); byteSum+=a2;
				a3 = getbyte(f); byteSum+=a3; /* address */
				address = (a1 << 16) + (a2 << 8) + a3;
				/*printf("%08x: ",address);*/
				errs += saveData(f,address+offset,len-4);
				/*printf("\n");*/
				break;
			}
			case '3' : {        /* data, 4 byte address */
				a1 = getbyte(f); byteSum+=a1;
				a2 = getbyte(f); byteSum+=a2;
				a3 = getbyte(f); byteSum+=a3;
				a4 = getbyte(f); byteSum+=a4;
				address = (a1 << 24) + (a2 << 16) + (a3 << 8) + a4;
				errs += saveData(f,address+offset,len-5);
				break;
			}
			case '4' : {	/* seems to be a special for the diags, len is byte len */
				i = fread(&s,1,len,f);
				s[i]=0;
				printf("unknown record S4 '%s'\n",s);
				break;
			}
			case '5' : {	/* record count */
				if (isDiag) {
					i = fread(&s,1,len,f);
					s[i]=0;
					printf("S5: '%s'\n",s);
				} else {
					skip(f,len);
				}
				break;
			}
			case '6' : {
				printf("unknown record S6\n");
				skip(f,len);
				break;
			}
			case '9' : {		/* end of file for S1, 16 bit start address */
				if (len >= 3) {
					a1 = getbyte(f); byteSum+=a1;
					a2 = getbyte(f); byteSum+=a2;
					len-=2;
					*startAddr = (a1 << 8) + a2;
					while (len > 0) {
						byteSum += getbyte(f);
						len--;
					}
					CHKSUM;
				} else {
					errs++;
					printf("Error: S9, length of record < 3\n");
					skip(f,len-1);
				}
				EXIT;
				break;
			}
			case '8' : {		/* end of file for S2, 3 byte address */
				if (len >= 4) {
					a1 = getbyte(f); byteSum+=a1;
					a2 = getbyte(f); byteSum+=a2;
					a3 = getbyte(f); byteSum+=a3; /* address */
					len-=3;
					*startAddr = (a1 << 16) + (a2 << 8) + a3;
					while (len > 0) {
						byteSum += getbyte(f);
						len--;
					}
					CHKSUM;
				} else {
					errs++;
					printf("Error: S8, length of record < 4\n");
					skip(f,len-1);
				}
				EXIT;
				break;
			}
			case '7' : {		/* end of file for S3, 4 byte address */
				if (len >= 5) {
					a1 = getbyte(f); byteSum+=a1;
					a2 = getbyte(f); byteSum+=a2;
					a3 = getbyte(f); byteSum+=a3; /* address */
					a4 = getbyte(f); byteSum+=a4;
					len-=4;
					*startAddr = (a1 << 24) + (a2 << 16) + (a3 << 8) + a4;
					while (len > 0) {
						byteSum += getbyte(f);
						len--;
					}
					CHKSUM;
				} else {
					errs++;
					printf("Error: S7, length of record < 5\n");
					skip(f,len-1);
				}
				EXIT;
				break;
			}
		}
		c = nextCh(f);
		while ((c == 13) || (c == 10)) c = nextCh(f);
	}
	EXIT;
}
