

/* This will become the standard letters.h, and we
 * will let configure build config.h
 *
 * For now, to enable builds, put common delcarations here prior
 * to renaming.
 *
 */

#define goto_xy(x, y)   move((y), (x))
void highlight(int);
void redraw(void);
#include <curses.h>
#include <unistd.h>
int underline(int on);
char * getword(void);
char * bonusword(void);

/*
 * configurable stuff in letters.  Most things here probably shouldn't need
 * to be changed but are here because someone may want to tinker with this
 * stuff to affect the way it performs on different systems.  The stuff
 * most likely to require changes is at the top of the file.
 */
#ifndef DICTIONARY
#define DICTIONARY "/usr/dict/words"
#endif

#ifndef HIGHSCORES
#define HIGHSCORES "letters.high"
#endif

/* TODO: get the actual screensize and handle SIGWINCH */
#define SCREENLENGTH 51
#define SCREENWIDTH  178

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
 * length of words in bonus round
 */
#define BONUSLENGTH	10

/*
 * minimum and maximum length of character strings chosen from
 * -s argument
 */

#define MINSTRING	3
#define MAXSTRING	8

