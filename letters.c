/*
 * letters.c: A simple game to help improve typing skills.
 *
 * copyright 1991 Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 * copyright 2018 David C Sterratt (david.c.sterratt@ed.ac.uk)
 */

# define CTRL(c)  (c & 037)

#define TRUE 1
#define FALSE 0

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <term.h>

#include "config.h"
#include "kinput.h"

struct s_word {
	struct s_word *nextword;
	int posx;
	int posy;
	int length;
	int drop;
	int matches;
	char word[1]; /* extensible */
};

static int move_words(void);

int key_pressed(void);
void update_scores(void);
void read_scores(void);
void show_scores(void);
void putword();
int  game();
void erase_word(struct s_word *wordp);
void status();
void new_level();
void banner();
struct s_word *newword();
struct s_word *searchstr(), *searchchar();
void kill_word();
int (*ding)(void); /* beep or flash */

void free();

/*
 * There are too many globals for my taste, but I took the easy way out in
 * a few places
 */
unsigned int score = 0;
int handicap = 1;
int level = 0;
int levels_played = -1;
unsigned int word_count = 0;
static int lives = 2;
static long delay;
struct s_word *words, *lastword, *prev_word;
int bonus = FALSE; /* to determine if we're in a bonus round */
int wpm = 0;
int letters = 0;
char *dictionary = DICTIONARY;
char *choice = NULL;
int   choicelen = 0;

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

/* Ensure the process is running on a tty. */
static void
check_tty(void)
{
	if (! isatty(STDIN_FILENO) || ! isatty(STDOUT_FILENO)) {
		fputs("This game can only be played on a terminal!\n", stderr);
		exit(EXIT_FAILURE);
	}
}


int
main(int argc, char **argv)
{
	char *progname;
	int foo;
	int newdict = 0;

	check_tty();
	ding = flash;
	progname = argv[0];

	/*
	 * check for options
	 */
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

	srand48(time(NULL));
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	clear();
	new_level();
	status();
	words = NULL;

	for(;;) {
		if(words == NULL) {
			lastword = words = newword((struct s_word *)NULL);
			prev_word = NULL;
			putword(lastword);
		}

		if(game() == 0) {
			move(0, SCREENLENGTH);
			break;
		}
	}

	read_scores();
	if(handicap == 1 && newdict == 0 && choice == NULL)
		update_scores();
	sleep(2);
	show_scores();
	endwin();
	printf("\n\nfinal: score = %u\twords = %u\t level = %d\n",
	       score, word_count, level);

	exit(0);
}

/*
 * move all words down 1 or more lines.
 * return the number of words that have fallen off the bottom of the screen
 */
static int
move_words(void)
{
	struct s_word  *wordp, *next;
	int  died;

	died = 0;
	for(wordp = words; wordp != NULL; wordp = next) {
		next = wordp->nextword;
		erase_word(wordp);
		wordp->posy += wordp->drop;

		if(wordp->posy >= SCREENLENGTH) {
			kill_word(wordp);
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
int game()
{
	int  key;
	long  i;
	int  died;
	struct s_word *curr_word, *temp_word;

	/*
	 * look to see if we already have a partial match, if not
	 * set the current word pointer to the first word
	 */
	for(curr_word = words; curr_word; curr_word = curr_word->nextword)
		if (curr_word->matches > 0)
			break;
	if (!curr_word)
		curr_word = words;
	while(curr_word->matches < curr_word->length) {
		for(i = 0; i < delay; i += PAUSE) {
			while(
				(curr_word->matches != curr_word->length) &&
				((key = key_pressed()) != -1)
			) {
				if(key == CTRL('L')) {
					redraw();
					continue;
				}
				if(key == CTRL('N')) {
					level++;
					delay = handicap * DELAY(level);
					if(delay < PAUSE)
						delay = PAUSE;
					status();
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
					goto_xy(SCREENWIDTH,SCREENLENGTH);
					fflush(stdout);
					curr_word->matches++;
					/*
					 * fill the word with characters to
					 * "explode" it.
					 */
					if(curr_word->matches >= curr_word->length)
						for (i = 0; i<curr_word->length; i++)
							curr_word->word[i] = '-';
					continue;

				} else if( (temp_word = searchstr(key,
							curr_word->word,
							curr_word->matches))) {
					erase_word(temp_word);
					temp_word->matches = curr_word->matches;
					curr_word->matches = 0;
					putword(curr_word);
					curr_word = temp_word;
					curr_word->matches++;
				} else if( (temp_word = searchchar(key))) {
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
				goto_xy(SCREENWIDTH,SCREENLENGTH);

				fflush(stdout);
			}
			usleep(PAUSE);
		}

		died = move_words();  /* NB: may invalidate curr_word */
		if (died > 0)
		{
			/*
			 * we only subtract lives if a word reaches the
			 * bottom in a normal round.  If a word reaches
			 * bottom during bonus play, just end the bonus
			 * round.
			 */
			if(bonus == FALSE)
				lives -= died;
			else if(died > 0)
				new_level();

			if (lives < 0)
				lives = 0;
			status();
			goto_xy(SCREENWIDTH,SCREENLENGTH);
			fflush(stdout);
			return (lives != 0);
		} else {
			goto_xy(SCREENWIDTH,SCREENLENGTH);
			fflush(stdout);
		}
		if((random() % ADDWORD) == 0) {
			lastword = newword(lastword);
			putword(lastword);
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
	score += curr_word->length + (2 * level);
	letters+= curr_word->length;
	word_count++;
	status();

	/*
	 * delete the completed word and revise pointers.
	 */
	kill_word(curr_word);

	/*
	 * increment the level if it's time.  If it's a bonus round, reward
	 * the person for finishing it.
	 */
	if(word_count % LEVEL_CHANGE == 0) {
		if(bonus == TRUE)
			score += 10 * level;
		new_level();
	}

	return 1;
}


/*
 * clear the screen and redraw it
 */
void redraw() {
	erase();
	status();
	fflush(stdout);
}

/*
 * display the status line in inverse video
 */
void status() {
	static char line[SCREENWIDTH];
	int  i;

	sprintf(line, "Score: %-7uLevel: %-3uWords: %-6uLives: %-3dWPM: %-4d",
		score, level, word_count, lives, wpm);

	/*
	 * fill the line with spaces
	 */
	for(i = strlen(line); i < SCREENWIDTH - 2; i++)
		line[i] = ' ';

	highlight(1);
	goto_xy(0, SCREENLENGTH - 1);
	printw("%s", line);
	highlight(0);
}

/*
 * do stuff to change levels.  This is where special rounds can be stuck in.
 */
void new_level()
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
	if(bonus == TRUE) {
		bonus = FALSE;
		banner("Bonus round finished");

		/*
		 * erase all existing words so we can go back to a normal
		 * round
		 */
		for(wordp = words; wordp != NULL; wordp = next) {
			next = wordp->nextword;
			kill_word(wordp);
		}

		status();
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
		bonus = TRUE;

		/*
		 * erase all existing words so we can have a bonus round
		 */
		for(wordp = words; wordp != NULL; wordp = next) {
			next = wordp->nextword;
			kill_word(wordp);
		}

		banner("Prepare for bonus words");
		lives++;
	}

	status();
}

/* Initialize a new word. */
struct s_word *
newword(struct s_word *wordp)
{
	struct s_word *nword;
	char  *word;
	int  length;

	word = (bonus == TRUE) ? bonusword() : getword();

	length = strlen(word);

	nword = malloc(sizeof *nword + length);
	if (nword == NULL) {
		endwin();
		perror("malloc");
		exit(1);
	}

	strncpy(nword->word, word, length);
	nword->length = length;
	nword->drop = length > 6 ? 1 : length > 3 ? 2 : 3;
	nword->matches = 0;
	nword->posx = random() % ((SCREENWIDTH - 1) - nword->length);
	nword->posy = 0;
	nword->nextword = NULL;

	if(wordp != NULL)
		wordp->nextword = nword;

	return nword;
}

/*
 * look at the first characters in each of the words to find one which
 * one matches the amount of stuff typed so far
 */
struct s_word *searchstr(key, str, len)
char *str;
int  len, key;
{
	struct s_word *wordp, *best;

	for(best = NULL, prev_word = NULL, wordp = words;
	    wordp != NULL;
	    prev_word = wordp,  wordp = wordp->nextword) {
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
struct s_word *searchchar(key)
char key;
{
	struct s_word	*wordp, *best;

	for(best = NULL, prev_word = NULL, wordp = words; wordp != NULL;
	    prev_word = wordp,  wordp = wordp->nextword) {
		if(wordp->word[0] == key
 		&& (!best || best->posy < wordp->posy))
			best = wordp;
	}

	return best;
}

void kill_word(wordp)
struct s_word *wordp;
{
	struct s_word *temp, *prev = NULL;

	/*
	 * check to see if the current word is the first one on our list
	 */
	if(wordp != words)
		for(prev = words, temp = words->nextword; temp != wordp;) {
			prev = temp;
			temp = temp->nextword;
		}

	if(prev != NULL) {
		prev->nextword = wordp->nextword;
	} else
		words = wordp->nextword;

	if(wordp->nextword != NULL)
		wordp->nextword = wordp->nextword->nextword;

	if(wordp == lastword)
		lastword = prev;

	free((char *)wordp);
}


/*
 * momentarily display a banner message across the screen and eliminate any
 * random keystrokes form the last round
 */
void
banner(const char *text)
{
	erase();
	goto_xy((SCREENWIDTH - strlen(text))/2, 10);
	printw("%s", text);
	goto_xy(SCREENWIDTH, SCREENLENGTH);
	refresh();
	sleep(3);
	erase();

	while(key_pressed() != -1)
		;
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
