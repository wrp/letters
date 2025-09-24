/*
 * letters.c: A simple game to help improve typing skills.
 *
 * copyright 1991 Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 * copyright 2018 David C Sterratt (david.c.sterratt@ed.ac.uk)
 */

/*
 * TODO
 * Add LICENSE file and figure out how to handle prior non-license
 * Build system (meson?)
 * Test suite
 * Refactor!
 * Rename config.h and build config.h at build time
 * signal handlers
 * quit cleanly with prompt
 */

# define CTRL(c)  (c & 0x1f)

#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <term.h>

#include "config.h"

struct s_word {
	struct s_word *nextword;
	int posx;
	int posy;
	int length;
	int drop;
	int matches;
	char word[32];
};

struct state;
static int move_words(struct state *);

void update_scores(unsigned);
int read_scores(void);
void show_scores(void);
void putword();
int  game();
void erase_word(struct s_word *wordp);
void status(struct state *);
void new_level(struct state *);
int banner(const char *, int);
struct s_word *newword();
struct s_word *searchstr(int key, char *str, int len, struct state *S);
struct s_word *searchchar(int, struct state *);
void kill_word(struct s_word *wordp, struct state *S);
int (*ding)(void); /* beep, flash, or no-op */

void free();

/*
 * There are too many globals for my taste, but I took the easy way out in
 * a few places
 */
struct state {
	struct s_word *words, *lastword, *prev_word;
	unsigned int score;
	jmp_buf jbuf;
};
int handicap = 1;
int level = 0;
int levels_played = -1;
unsigned int word_count = 0;
static int lives = 2;
static long delay;
int bonus = 0; /* to determine if we're in a bonus round */
int wpm = 0;
int letters = 0;
char *dictionary = DICTIONARY;
char *choice = NULL;
int   choicelen = 0;
int newdict = 0;

void
usage(const char *progname)
{
	printf(
		"Usage: %s [-q|-b] [-h] [-H#] [-l#] [-ddictionary] "
		"[-sstring]\n",
		progname
	);
	exit(0);
}

int no_op(void) { return 0; }  /* Do nothing function */
/* Ensure the process is running on a tty. */
static void
check_tty(void)
{
	if (! isatty(STDIN_FILENO) || ! isatty(STDOUT_FILENO)) {
		fputs("This game can only be played on a terminal!\n", stderr);
		exit(EXIT_FAILURE);
	}
}


static void
intrrpt(struct state *S)
{
	switch(banner("Are you sure you want to quit?", 0)) {
	case 'y':
	case 'Y':
		longjmp(S->jbuf, 1);
	default:
		erase();
		status(S);
		refresh();
	}
}

static void
parse_cmd_line(int argc, char **argv)
{
	char *progname;
	progname = argv[0];
	while(*++argv) {
		if((*argv)[0] == '-') {
			switch((*argv)[1]) {
				case 'b':
					ding = beep;
					break;
				case 'q':
					ding = flash;
					break;
				case 'H':
					sscanf(&argv[0][2], "%d", &handicap);
					if (handicap < 1) handicap = 1;
					break;
				case 'h':
					read_scores();
					show_scores();
					exit(0);
					break;
				case 'l':
					sscanf(&argv[0][2], "%d", &level);
					if(DELAY(level) <= PAUSE) {
						fprintf(stderr, "You may not start at level %d\n", level);
						exit(0);
					}
					break;
				case 'd':
					if ((*argv)[2] ) {
						dictionary = *argv+2;
						newdict = 1;
					}
					break;
				case 's':
					if ((*argv)[2] ) {
						choice = *argv+2;
						choicelen = strlen(choice);
					}
					break;
				default:
					usage(progname);
			}
		} else {
			usage(progname);
		}
	}
}


static void
init(struct state *S, int argc, char **argv)
{
	check_tty();
	unsetenv("COLUMNS");
	unsetenv("LINES");

	ding = no_op;
	parse_cmd_line(argc, argv);

	srand48(time(NULL));
	initscr();
	raw();
	curs_set(0);
	noecho();
	keypad(stdscr, 1);
	nodelay(stdscr, 1);
	clear();
	new_level(S);
	status(S);
	S->words = NULL;
}


int
main(int argc, char **argv)
{
	struct state S[1] = {{0}};

	init(S, argc, argv);

	if (setjmp(S->jbuf)) {
		goto exit;
	}
	for(;;) {
		if(S->words == NULL) {
			S->lastword = S->words = newword(NULL);
			S->prev_word = NULL;
			putword(S->lastword);
		}

		if(game(S) == 0) {
			move(0, LINES);
			break;
		}
	}

	read_scores();
	if(handicap == 1 && newdict == 0 && choice == NULL)
		update_scores(S->score);
	sleep(2);
	show_scores();
exit:
	endwin();
	printf("\n\nfinal: score = %u\twords = %u\t level = %d\n",
		S->score, word_count, level);

	exit(0);
}

/*
 * move all words down 1 or more lines.
 * return the number of words that have fallen off the bottom of the screen
 */
static int
move_words(struct state *S)
{
	struct s_word  *wordp, *next;
	int  died;

	died = 0;
	for(wordp = S->words; wordp != NULL; wordp = next) {
		next = wordp->nextword;
		erase_word(wordp);
		wordp->posy += wordp->drop;

		if(wordp->posy >= LINES) {
			kill_word(wordp, S);
			died++;
		}
		else {
			putword(wordp);
		}
	}
	return died;
}

/*
 * erase a word on the screen by printing the correct number of blanks
 */
void erase_word(struct s_word *wordp)
{
	int i;

	goto_xy(wordp->posx, wordp->posy);
	for(i = 0; i < wordp->length; i++)
		printw("%c", ' ');
}

/* write the word to the screen with already typed letters highlighted */
void
putword(struct s_word *wordp)
{
	int i;

	goto_xy(wordp->posx, wordp->posy);

	/*
	 * print the letters in the word that have so far been matched
	 * correctly.
	 */
	highlight(1);
	for(i = 0; i < wordp->matches; i++)
		printw("%c", wordp->word[i]);
	highlight(0);

	/* print the rest of the word  */
	for(i = wordp->matches; i < wordp->length; i++)
		printw("%c", wordp->word[i]);
}

/*
 * Here's the main routine of the actual game.
 */
int
game(struct state *S)
{
	int  key;
	long  i;
	int  died;
	struct s_word *curr_word, *temp_word;

	/*
	 * look to see if we already have a partial match, if not
	 * set the current word pointer to the first word
	 */
	for(curr_word = S->words; curr_word; curr_word = curr_word->nextword)
		if (curr_word->matches > 0)
			break;
	if (!curr_word)
		curr_word = S->words;
	while(curr_word->matches < curr_word->length) {
		for(i = 0; i < delay; i += PAUSE) {
			while(
				(curr_word->matches != curr_word->length) &&
				((key = getch()) != ERR)
			) {
				if(
					key == CTRL('L') ||
					key == KEY_RESIZE
				) {
					erase();
					status(S);
					continue;
				}
				if(key == CTRL('N')) {
					level++;
					delay = handicap * DELAY(level);
					if(delay < PAUSE)
						delay = PAUSE;
					status(S);
					continue;
				}
				if(key == CTRL('C')) {
					intrrpt(S);
					continue;
				}
				/*
				 * This stuff deals with collecting letters
				 * for a word that has already been
				 * started.  It's kind of clumsy the way
				 * it's being done now and should be
				 * cleaned up, but the obvious combination
				 * of erase() and putword() generate too
				 * much output to be used at 2400 baud.  (I
				 * can't play too often at work)
				 */
				if(curr_word->matches > 0 &&
				   key == curr_word->word[curr_word->matches]) {
					goto_xy(curr_word->posx + curr_word->matches, curr_word->posy);
					highlight(1);
					printw("%c", curr_word->word[curr_word->matches]);
					highlight(0);
					goto_xy(COLS, LINES);
					curr_word->matches++;
					/*
					 * fill the word with characters to
					 * "explode" it.
					 */
					if(curr_word->matches >= curr_word->length)
						for (i = 0; i<curr_word->length; i++)
							curr_word->word[i] = '-';
					continue;

				} else if ((temp_word = searchstr(
					key,
					curr_word->word,
					curr_word->matches,
					S
				) )) {
					erase_word(temp_word);
					temp_word->matches = curr_word->matches;
					curr_word->matches = 0;
					putword(curr_word);
					curr_word = temp_word;
					curr_word->matches++;
				} else if( (temp_word = searchchar(key, S))) {
					erase_word(temp_word);
					curr_word->matches = 0;
					putword(curr_word);
					curr_word = temp_word;
					curr_word->matches++;
				} else {
					ding();
					curr_word->matches = 0;
				}
				erase_word(curr_word);
				putword(curr_word);
				goto_xy(COLS, LINES);

			}
			/* TODO: use an itimer for more precision */
			usleep(PAUSE);
		}

		died = move_words(S);  /* NB: may invalidate curr_word */
		if (died > 0)
		{
			/*
			 * we only subtract lives if a word reaches the
			 * bottom in a normal round.  If a word reaches
			 * bottom during bonus play, just end the bonus
			 * round.
			 */
			if (! bonus) {
				lives -= died;
			} else if (died > 0) {
				new_level(S);
			}
			if (lives < 0) {
				lives = 0;
			}
			status(S);
			goto_xy(COLS, LINES);
			return (lives != 0);
		}
		refresh();
		if((random() % ADDWORD) == 0) {
			S->lastword = newword(S->lastword);
			putword(S->lastword);
		}
	}

	/*
	 * all letters in the word have been correctly typed.
	 */

	/*
	 * erase the word
	 */
	if(curr_word->length == curr_word->matches) {
		ding(); ding();
		erase_word(curr_word);
	}

	/*
	 * add on an appropriate score.
	 */
	S->score += curr_word->length + (2 * level);
	letters+= curr_word->length;
	word_count++;
	status(S);

	/*
	 * delete the completed word and revise pointers.
	 */
	kill_word(curr_word, S);

	/*
	 * increment the level if it's time.  If it's a bonus round, reward
	 * the person for finishing it.
	 */
	if(word_count % LEVEL_CHANGE == 0) {
		if (bonus)
			S->score += 10 * level;
		new_level(S);
	}

	return 1;
}


/* Display the status line. */
void
status(struct state *S)
{
	goto_xy(COLS / 2 - 28, 0);
	highlight(1);
	printw("Score: %-7u", S->score);
	printw("Level: %-3u", level);
	printw("Words: %-6u", word_count);
	printw("Lives: %-3d", lives);
	printw("WPM: %-4d", wpm);
	clrtoeol();
	highlight(0);
}

/*
 * do stuff to change levels.  This is where special rounds can be stuck in.
 */
void
new_level(struct state *S)
{
	struct s_word *next, *wordp;
	static time_t last_time = 0L;
	time_t  curr_time;

	/*
	 * update the words per minute
	 */
	time(&curr_time);
	wpm = (letters / 5) / ((curr_time - last_time) / 60.0);
	last_time = curr_time;
	letters = 0;

	/*
	 * if we're inside a bonus round we don't need to change anything
	 * else so just take us out of the bonus round and exit this routine
	 */
	if (bonus) {
		bonus = 0;
		banner("Bonus round finished", 3);

		/*
		 * erase all existing words so we can go back to a normal
		 * round
		 */
		for(wordp = S->words; wordp != NULL; wordp = next) {
			next = wordp->nextword;
			kill_word(wordp, S);
		}

		status(S);
		return;
	}

	levels_played++;

	/*
	 * If you start at a level other than 1, the level does not
	 * actually change until you've completed a number of levels equal
	 * to the starting level.
	 */
	if(level <= levels_played)
		level++;

	delay = handicap * DELAY(level);

	/*
	 * no one should ever reach a level where there is no delay, but
	 * just to be safe ...
	 */
	if(delay < PAUSE)
		delay = PAUSE;

	if((levels_played % 3 == 0) && (levels_played != 0)) {
		bonus = 1;

		/*
		 * erase all existing words so we can have a bonus round
		 */
		for(wordp = S->words; wordp != NULL; wordp = next) {
			next = wordp->nextword;
			kill_word(wordp, S);
		}

		banner("Prepare for bonus words", 3);
		lives++;
	}

	status(S);
}

/* Initialize a new word. */
struct s_word *
newword(struct s_word *wordp)
{
	struct s_word *nword;
	int  length;
	size_t s = sizeof nword->word;

	nword = malloc(sizeof *nword);
	char *word = nword->word;

	length = (bonus) ? bonusword(word, s) : getword(word, s);

	if (nword == NULL) {
		endwin();
		perror("malloc");
		exit(1);
	}

	nword->length = length;
	nword->drop = length > 6 ? 1 : length > 3 ? 2 : 3;
	nword->matches = 0;
	nword->posx = random() % ((COLS - 1) - nword->length);
	nword->posy = 1;
	nword->nextword = NULL;

	if(wordp != NULL)
		wordp->nextword = nword;

	return nword;
}

/*
 * look at the first characters in each of the words to find one which
 * one matches the amount of stuff typed so far
 */
struct s_word *
searchstr(int key, char *str, int len, struct state *S)
{
	struct s_word *wordp, *best;

	for(best = NULL, S->prev_word = NULL, wordp = S->words;
		wordp != NULL;
		S->prev_word = wordp,  wordp = wordp->nextword
	) {
		if(wordp->length > len
		&& strncmp(wordp->word, str, len) == 0
		&& wordp->word[len] == key
		&& (!best || best->posy < wordp->posy))
			best = wordp;
	}

	return best;
}


/*
 * look at the first character in each of the words to see if any match the
 * one that was typed.
 */
struct s_word *
searchchar(int key, struct state *S)
{
	struct s_word	*wordp, *best;

	for(best = NULL, S->prev_word = NULL, wordp = S->words;
		wordp != NULL;
		S->prev_word = wordp,  wordp = wordp->nextword
	) {
		if(wordp->word[0] == key
 		&& (!best || best->posy < wordp->posy))
			best = wordp;
	}

	return best;
}

void
kill_word(struct s_word *wordp, struct state *S)
{
	struct s_word *temp, *prev = NULL;

	/*
	 * check to see if the current word is the first one on our list
	 */
	if(wordp != S->words)
		for(prev = S->words, temp = S->words->nextword; temp != wordp;) {
			prev = temp;
			temp = temp->nextword;
		}

	if(prev != NULL) {
		prev->nextword = wordp->nextword;
	} else
		S->words = wordp->nextword;

	if(wordp->nextword != NULL)
		wordp->nextword = wordp->nextword->nextword;

	if(wordp == S->lastword)
		S->lastword = prev;

	free((char *)wordp);
}


/*
 * momentarily display a banner message across the screen
 */
int
banner(const char *text, int delay_sec)
{
	int c = ERR;

	erase();
	goto_xy((COLS - strlen(text))/2, 10);
	printw("%s", text);
	refresh();
	if (delay_sec) {
		sleep(delay_sec);
	} else {
		timeout(-1);
		c = getch();
		timeout(0);
	}
	erase();
	return c;
}


void
highlight(int s)
{
	switch(s) {
	case 0: attroff(A_STANDOUT); break;
	case 1: attron(A_STANDOUT); break;
	}
}


int
underline(int on)
{
	return on ? attron(A_UNDERLINE) : attroff(A_UNDERLINE);
}
