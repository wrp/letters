#include "config.h"

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
	unsigned level;
	int lives;
	struct word *words, *lastword;
	struct word *current;  /* Word user is currently typing */
	struct score score;
	jmp_buf jbuf;
	long delay;
	int handicap;
	bool bonus;   /* true if we're in a bonus round */
};



/* number of words to be completed before level change */
#define LEVEL_CHANGE 15

/* number of levels between bonus rounds */
#define LVL_PER_BONUS 3

/*
 * length of pause before reading keyboard again (in usecs).  There has to
 * be some pause.
 */
# define PAUSE  10000

/*
 * This is how likely it is that another word will appear on the screen
 * while words are falling.  there isa 1/ADDWORD chance of a new word
 */
#define ADDWORD  6

/*
 * minimum and maximum length of character strings chosen from
 * -s argument
 */

#define MINSTRING 3
#define MAXSTRING 8
