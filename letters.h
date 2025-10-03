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
	unsigned char len;  /* .len == strlen(.data) - 1 */
};

struct dictionary {
	struct string *index;
	size_t cap;
	size_t len;
};

struct word {
	struct word *next;
	float x;     /* horizontal coordinate of position */
	int y;       /* vertical coordinate of position */
	int speed;   /* number of ticks per move */
	int offset;  /* Added to ticks to control timing */
	int matches; /* Length of matching prefix */
	int killed;  /* word has been marked for deletion */
	int lateral; /* control lateral motion */
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
	unsigned ms_per_tick;
	int handicap;
	int levels_completed;
	time_t start_time; /* time the current level started */
	unsigned letters; /* number of keys correctly typed thislevel */
	int wpm;
	bool bonus;   /* true if we're in a bonus round */
	char *dictionary; /* Path to dictionary file */
	char *choice; /* String from which to construct random strings */
	float addword; /* Chance of getting a new word each tick */
};

void redraw(void);
struct string getword(void);
struct string bonusword(void);
struct score_rec *next_score(char *, size_t);
typedef void *(*reallocator)(void *, size_t);
void initialize_dictionary(char *path, char *, reallocator);
void free_dictionaries(void);


/* number of words to be completed before level change */
#define LEVEL_CHANGE 15

/* number of levels between bonus rounds */
#define LVL_PER_BONUS 3

/*
 * minimum and maximum length of character strings chosen from
 * -s argument
 */

#define MINSTRING 3
#define MAXSTRING 8
