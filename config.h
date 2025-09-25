
#include <assert.h>
#include <curses.h>
#include <math.h>
#include <pwd.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <term.h>
#include <time.h>
#include <unistd.h>

#define goto_xy(x, y)   move((y), (x))
void highlight(int);
void redraw(void);
int underline(int on);
size_t getword(char *buf, size_t bufsiz);
size_t bonusword(char *buf, size_t bufsiz);

struct score {
	unsigned points;
	unsigned words;
	unsigned letters;
};
struct state {
	int level;
	struct s_word *words, *lastword;
	struct score score;
	jmp_buf jbuf;
	long delay;
	int handicap;
};

#define DICTIONARY "/usr/share/dict/words"

#ifndef HIGHSCORES
#define HIGHSCORES "letters.high"
#endif

/*
 * initial delay in usecs before words move to the next line
 */
#define START_DELAY	750000

/*
 * this implements "graduated" (non-linear) decreasing delay times:
 * each level, delay gets reduced by smaller and smaller amounts
 * (eventually, when delay would get below PAUSE, it is simply set to PAUSE)
 *
 * if you change START_DELAY or DELAY_CHANGE, DECEL must be tuned carefully,
 * otherwise DELAY(lev) will drop suddenly to PAUSE at some point
 */
#define DELAY_CHANGE	60000
#define DECEL		1200
#define DELAY(lev)	( (((long)(lev))*DECEL > DELAY_CHANGE/2) ? PAUSE :\
			  (START_DELAY-(DELAY_CHANGE-(lev)*DECEL)*(lev)) )

/*
 * number of words to be completed before level change
 */
#define LEVEL_CHANGE	15

/*
 * length of pause before reading keyboard again (in usecs).  There has to
 * be some pause.
 */
# define PAUSE		10000

/*
 * This is how likely it is that another word will appear on the screen
 * while words are falling.  there isa 1/ADDWORD chance of a new word
 */
#define ADDWORD		6

/*
 * minimum and maximum length of character strings chosen from
 * -s argument
 */

#define MINSTRING	3
#define MAXSTRING	8

