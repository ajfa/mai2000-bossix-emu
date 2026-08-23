/***************************************************************************
 *            util.c
 *
 *  Tue November 22 21:23:11 2011
 *  Copyright  2011  Armin Diehl
 *  <ad@ardiehl.de>
 ****************************************************************************/
/*
 * util.c
 *
 * Copyright (C) 2011 - Armin Diehl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

/*#include <stdlib.h>*/
#include <string.h>
#include <ctype.h>
#include "util.h"


/*
int kbhit(void) {
       struct termios term, oterm;
       int fd = 0;
       int c = 0;

       tcgetattr(fd, &oterm);
       memcpy(&term, &oterm, sizeof(term));
       term.c_lflag = term.c_lflag & (!ICANON);
       term.c_cc[VMIN] = 0;
       term.c_cc[VTIME] = 0;
       tcsetattr(fd, TCSANOW, &term);
       c = getchar();
       tcsetattr(fd, TCSANOW, &oterm);
       if (c != -1)
       ungetc(c, stdin);
       return ((c != -1) ? 1 : 0);
}*/
#if 0
int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;
  int fd = 0;

  tcgetattr(fd, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(fd, TCSANOW, &newt);
  oldf = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(fd, TCSANOW, &oldt);
  fcntl(fd, F_SETFL, oldf);

  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}

#endif
int getch_noecho(void)
{
   static int ch = -1, fd = 0;
   struct termios neu, alt;

   fd = fileno(stdin);
   tcgetattr(fd, &alt);
   neu = alt;
   neu.c_lflag &= ~(ICANON|ECHO);
   tcsetattr(fd, TCSANOW, &neu);
   ch = getchar();
   tcsetattr(fd, TCSANOW, &alt);
   return ch;
}


int kbhit(void)
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

void kb_raw(void)
{
    struct termios ttystate;

    tcgetattr(STDIN_FILENO, &ttystate);
	ttystate.c_lflag &= ~ICANON;
	ttystate.c_lflag &= ~ECHO;
	ttystate.c_lflag &= ~ISIG;
	ttystate.c_cc[VMIN] = 1; /* minimum of number input read */
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}


void kb_normal(void)
{
	struct termios ttystate;

	tcgetattr(STDIN_FILENO, &ttystate);
	ttystate.c_lflag |= ICANON;
	ttystate.c_lflag |= ECHO;
	ttystate.c_lflag |= ISIG;
	tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}


int getch(void)
{
	char c;
	ssize_t len;
	do {
	  len = read(STDIN_FILENO, (void *)&c, 1);
	} while (len < 1);
	if(c==10) c=13;
	return c;
}

/* Input string from console, input terminated by
 * any char in exitChars */
char inputString (char * s, int maxLen, const char * exitChars)
{
	char c;
	int i;
	*s = 0;

	do {
      c = getch_noecho();
	  i = strlen(s);

	  if ((c == KEY_BS) || (c == KEY_DEL)) {
		  if (i > 0) {
			  i--; printf("%c %c",8,8);
			  s[i]=0;
		  }
	  } else
	  if (!(strchr(exitChars,c)) & (i < maxLen)) {
		  if (c >= ' ') {
		      s[i] = c; s[++i] = 0;
		      putchar(c);
		  }
	  }
	} while (!(strchr(exitChars,c)));
	switch (c) {
		case ' ': { putchar(' '); break; }
		case 13 : { printf("\r\n"); break; }
	}
    return c;
}


/* returns 1 on succsess */
int xtoui(const char* xs, unsigned int* result)
{
 size_t szlen = strlen(xs);
 int i, xv, fact;

 if (szlen > 0)
 {
  if (szlen>8) return 0; /* limut to 32 bit */

  *result = 0;
  fact = 1;

  for(i=szlen-1; i>=0 ;i--)
  {
   if (isxdigit(*(xs+i)))
   {
    if (*(xs+i)>='a') xv = ( *(xs+i) - 'a') + 10;
    else if ( *(xs+i) >= 'A') xv = (*(xs+i) - 'A') + 10;
    else xv = *(xs+i) - '0';
    *result += (xv * fact);
    fact *= 16;
   } else return 0;
  }
 } else return 0;
 return 1;
}


/* returns hex digit for lower 4 bits */
char hexNibble (unsigned int i)
{
	char digits[16] = "0123456789abcdef";
	return digits[i & 0x0f];
}

int file_exists(char * filename) {
    if (access(filename, 0 ) == 0) return 1;
    else return 0;
}


int is_directory(char * filename) {
    struct stat status;

    if (!(file_exists(filename))) return 0;

    stat(filename, &status);

    if (status.st_mode & S_IFDIR) return 1;
    return 0;
}

int file_getSize(char * filename) {
    struct stat status;

    if (!(file_exists(filename))) return -1;

    stat(filename, &status);

    return status.st_size;
}
