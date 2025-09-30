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

extern char *score_header;

struct string {
	char *data;
	size_t cap;
	size_t len;  /* Includes '0'.  eg strlen() - 1 */
};

void highlight(int);
void redraw(void);
int underline(int on);
struct string getword(void);
struct string bonusword(void);
struct score_rec;
struct score_rec *next_score(char *buf, size_t siz);
typedef void *(*reallocator)(void *, size_t);
void initialize_dictionary(char *path, reallocator r);

struct word {
	struct word *next;
	int x;       /* horizontal coordinate of position */
	int y;       /* vertical coordinate of position */
	int length;  /* strlen of word */
	int drop;    /* number of lines to move per turn */
	int matches; /* Length of matching prefix */
	int killed;  /* word has been marked for deletion */
	struct string word;
};
struct score {
	unsigned points;
	unsigned words;
};
struct score_rec {
	char	name[9];
	int	level, words, score;
};

struct state {
	unsigned level;
	int lives;
	struct word *words; /* list of words in play */
	struct word *free; /* list of unused words */
	struct score score;
	jmp_buf jbuf;
	long delay;
	int handicap;
	time_t start_time; /* time the current level started */
	unsigned letters; /* number of keys correctly typed thislevel */
	int wpm;
	bool bonus;   /* true if we're in a bonus round */
	char *dictionary; /* Path to dictionary file */
};



/* number of words to be completed before level change */
#define LEVEL_CHANGE 15

/* number of levels between bonus rounds */
#define LVL_PER_BONUS 3

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
