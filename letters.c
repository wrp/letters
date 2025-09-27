/*
 * letters.c: A simple game to help improve typing skills.
 *
 * copyright 1991 Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 * copyright 2018 David C Sterratt (david.c.sterratt@ed.ac.uk)
 */

/*
 * TODO
 * Build system (meson?)
 * Test suite
 * Refactor!
 * "Explode" words -- display with "----" or "****" before killing
 * When getting a top 10 score, highlight the current score in the display
 * Use itimer and clean up all the pause/delay cruft
 */

# define CTRL(c)  (c & 0x1f)

#include "letters.h"
#include <signal.h>
#include <sys/time.h>

struct word {
	struct word *nextword;
	int posx;
	int posy;
	int length;
	int drop;
	int matches;
	char word[32];
};

static int move_words(struct state *);
static void set_timer(struct state *);
static void set_handlers(void);

void update_scores(char *, struct score *, unsigned);
int read_scores(char *);
char * next_score(char *buf, size_t siz);
void show_scores(struct state *S);
void putword();
static void game(struct state *);
void erase_word(struct word *wordp);
void status(struct state *);
void new_level(struct state *);
int banner(const char *, int);
static struct word *newword(struct word *, bool);
struct word *searchstr(int key, char *str, int len, struct state *S);
struct word *searchchar(int, struct state *);
void kill_word(struct word *wordp, struct state *S);
int (*ding)(void); /* beep, flash, or no-op */

volatile sig_atomic_t received_signal;

typedef void (*handler)(int, siginfo_t *, void *);

/*
 * There are too many globals for my taste, but I took the easy way out in
 * a few places
 */
int levels_played = -1;
int wpm = 0;
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

void
assign_signal(int s, siginfo_t *i, void *v)
{
	(void)i;
	(void)v;
	received_signal = s;
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


static void
intrrpt(struct state *S)
{
	switch(banner("Are you sure you want to quit?", 0)) {
	case 'y':
	case 'Y':
	case CTRL('C'):
		longjmp(S->jbuf, 1);
	default:
		erase();
		status(S);
	}
}


/* milliseconds to pause before moving words down */
int
DELAY(unsigned level)
{
	/* START_DELAY - level * (DELAY_CHANGE - (level * DECEL)) */
	long lut[] = {
		750000, 691200, 634800, 580800, 529200, 480000,
		433200, 388800, 346800, 307200, 270000, 235200,
		202800, 172800, 145200, 120000, 97200, 76800,
		58800, 43200, 30000, 19200, 10800
	};
	return level < sizeof lut / sizeof *lut ? lut[level] : 10000;
}


static void
handle_argument(struct state *S, char **argv)
{
	char *end;
	char *arg = *argv;

	switch(arg[1]) {
	case 'b': ding = beep; break;
	case 'q': ding = flash; break;
	case 'H':
		S->handicap = (int)strtol(arg + 2, &end, 0);
		if( *end || S->handicap < 1 ){
			fprintf(stderr, "Invalid handicap %s\n", arg + 2);
			exit(1);
		}
		break;
	case 'h':
		read_scores(HIGHSCORES);
		for (char s[64]; next_score(s, sizeof s); ) {
			printf("%s\n", s);
		}
		exit(0);
		break;
	case 'l':
		S->level = (int)strtol(arg + 2, &end, 0);
		if( *end || (DELAY(S->level) <= PAUSE) ) {
			fprintf(stderr, "Invalid level %s\n", arg + 2);
			exit(1);
		}
		break;
	case 'd':
		dictionary = arg + 2;
		newdict = 1;
		if (! arg[2]) {
			fprintf(stderr, "-d option requires an argument\n");
			exit(1);
		}
		break;
	case 's':
		if (! arg[2]) {
			fprintf(stderr, "-s option requires an argument\n");
			exit(1);
		}
		choice = arg + 2;
		choicelen = strlen(choice);
		break;
	default:
		fprintf(stderr, "Unknown option: -%c\n", arg[1]);
		exit(1);
	}
}


static void
parse_cmd_line(int argc, char **argv, struct state *S)
{
	char *progname;
	progname = argv[0];
	while(*++argv) {
		if((*argv)[0] == '-') {
			handle_argument(S, argv);
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
	S->handicap = 1;
	parse_cmd_line(argc, argv, S);

	set_handlers();
	srand48(time(NULL));
	initscr();
	raw();
	curs_set(0);
	noecho();
	keypad(stdscr, 1);
	clear();
	new_level(S);
	status(S);
	S->words = NULL;
	S->lives = 2;
}


int
main(int argc, char **argv)
{
	struct state S[1] = {{0}};

	init(S, argc, argv);

	if (setjmp(S->jbuf)) {
		goto exit;
	}
	do {
		if (S->words == NULL) {
			S->lastword = S->words = newword(NULL, S->bonus);
			putword(S->lastword);
		}
		game(S);
	} while (S->lives > 0);

exit:
	set_timer(NULL);
	read_scores(HIGHSCORES);
	if (S->handicap == 1 && newdict == 0 && choice == NULL)
		update_scores(HIGHSCORES, &S->score, S->level);
	show_scores(S);
	endwin();

	return 0;
}

/*
 * move all words down 1 or more lines.
 * return the number of words that have fallen off the bottom of the screen
 */
static int
move_words(struct state *S)
{
	struct word  *w, *next;
	int  died = 0;

	for (w = S->words; w != NULL; w = next) {
		next = w->nextword;
		erase_word(w);
		w->posy += w->drop;

		if (w->posy >= LINES) {
			kill_word(w, S);
			died += 1;
		} else {
			putword(w);
		}
	}

	if (died > 0) {
		/*
		 * subtract lives if a word reaches the
		 * bottom in a normal round.  If a word reaches
		 * bottom during bonus play, just end the bonus
		 * round.
		 */
		if (! S->bonus) {
			S->lives -= died;
		} else if (died > 0) {
			new_level(S);
		}
		if (S->lives < 0) {
			S->lives = 0;
		}
	}
	return died;
}

/*
 * erase a word on the screen by printing the correct number of blanks
 */
void erase_word(struct word *wordp)
{
	int i;

	goto_xy(wordp->posx, wordp->posy);
	for(i = 0; i < wordp->length; i++)
		printw("%c", ' ');
}

/* write the word to the screen with already typed letters highlighted */
void
putword(struct word *wordp)
{
	int idx = wordp->matches;
	int c = wordp->word[idx];

	assert(idx <= wordp->length);
	goto_xy(wordp->posx, wordp->posy);
	highlight(1);
	wordp->word[idx] = '\0';
	printw("%s", wordp->word);
	wordp->word[idx] = c;
	highlight(0);
	printw("%s", wordp->word + wordp->matches);
}


static int
handle_ctrl_key(struct state *S, int key)
{
	switch(key) {
	case CTRL('L'):
	case KEY_RESIZE:
		erase();
		status(S);
		break;
	case CTRL('N'):
		S->level += 1;
		S->delay = S->handicap * DELAY(S->level);
		if(S->delay < PAUSE)
			S->delay = PAUSE;
		status(S);
		break;
	case CTRL('C'):
		intrrpt(S);
		break;
	default:
		return 0;
	}
	return 1;
}

/*
 * Find any word that has a partial match, or return S->words
 */
static struct word *
find_match(const struct state *S)
{
	struct word *this;
	for (this = S->words; this; this = this->nextword) {
		if (this->matches > 0) {
			return this;
		}
	}
	return S->words;
}


/* Process the user keystrokes until word matched or signal received */
static void
process_keys(struct state *S)
{
	int  key;
	struct word *temp_word;
	while(
		(S->current->matches != S->current->length) &&
		((key = getch()) != ERR) &&
		! received_signal
	) {
		if (handle_ctrl_key(S, key)) {
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
		if(S->current->matches > 0 &&
			key == S->current->word[S->current->matches])
		{
			int x = S->current->posx;
			int y = S->current->posy;
			goto_xy(x + S->current->matches, y);
			highlight(1);
			printw("%c", key);
			highlight(0);
			S->current->matches += 1;
			continue;
		} else if ((temp_word = searchstr(
			key,
			S->current->word,
			S->current->matches,
			S
		) )) {
			erase_word(temp_word);
			temp_word->matches = S->current->matches;
			S->current->matches = 0;
			putword(S->current);
			S->current = temp_word;
			S->current->matches++;
		} else if( (temp_word = searchchar(key, S))) {
			erase_word(temp_word);
			S->current->matches = 0;
			putword(S->current);
			S->current = temp_word;
			S->current->matches++;
		} else {
			ding();
			S->current->matches = 0;
		}
		erase_word(S->current);
		putword(S->current);
	}
}


/* Word has been successfully typed.  Increment score and remove. */
static void
finalize_word(struct state *S)
{
	assert (S->current->length == S->current->matches);
	erase_word(S->current);
	S->score.points += S->current->length + (2 * S->level);
	S->score.letters += S->current->length;
	S->score.words += 1;
	kill_word(S->current, S);
}


/* Set an interval timer in ms */
static void
set_timer(struct state *S)
{
	unsigned long delay_msec = S ? S->delay / 1000 : 0;
	struct timeval tp = {
		.tv_sec = 0,
		.tv_usec = delay_msec * 1000
	};
	while (tp.tv_usec > 999000) {
		tp.tv_sec += 1;
		tp.tv_usec -= 1000000;
	}
	struct itimerval t = {
		.it_interval = tp,
		.it_value = tp
	};

	if (setitimer(ITIMER_REAL, &t, NULL)) {
		endwin();
		perror("setitimer");
		exit(1);
	}
	timeout(delay_msec);
}


static void
game(struct state *S)
{
	long  i;

	S->current = find_match(S);
	while(S->current->matches < S->current->length) {
		set_timer(S);
		process_keys(S);
		received_signal = 0;
		refresh();

		if (move_words(S)) {
			status(S);
			return;
		}
		if((random() % ADDWORD) == 0) {
			S->lastword = newword(S->lastword, S->bonus);
			putword(S->lastword);
		}
	}

	finalize_word(S);
	status(S);

	/*
	 * increment the level if it's time.  If it's a bonus round, reward
	 * the person for finishing it.
	 */
	if(S->score.words % LEVEL_CHANGE == 0) {
		if (S->bonus)
			S->score.points += 10 * S->level;
		new_level(S);
	}
}


/* Display the status line. */
void
status(struct state *S)
{
	goto_xy(COLS / 2 - 28, 0);
	highlight(1);
	printw("Score: %-7u", S->score.points);
	printw("Level: %-3u", S->level);
	printw("Words: %-6u", S->score.words);
	printw("Lives: %-3d", S->lives);
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
	struct word *next, *wordp;
	static time_t last_time = 0L;
	time_t  curr_time;

	/*
	 * update the words per minute
	 */
	time(&curr_time);
	wpm = (S->score.letters / 5) / ((curr_time - last_time) / 60.0);
	last_time = curr_time;
	S->score.letters  = 0;

	/*
	 * if we're inside a bonus round we don't need to change anything
	 * else so just take us out of the bonus round and exit this routine
	 */
	if (S->bonus) {
		S->bonus = false;
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
	if(S->level <= levels_played)
		S->level += 1;

	S->delay = S->handicap * DELAY(S->level);

	/*
	 * no one should ever reach a level where there is no delay, but
	 * just to be safe ...
	 */
	if(S->delay < PAUSE)
		S->delay = PAUSE;

	if((levels_played % LVL_PER_BONUS == 0) && (levels_played != 0)) {
		S->bonus = true;

		/*
		 * erase all existing words so we can have a bonus round
		 */
		for(wordp = S->words; wordp != NULL; wordp = next) {
			next = wordp->nextword;
			kill_word(wordp, S);
		}

		banner("Prepare for bonus words", 3);
		S->lives += 1;
	}

	status(S);
}

/* Initialize a new word. */
struct word *
newword(struct word *wordp, bool bonus)
{
	struct word *nword;
	int  length;
	size_t s = sizeof nword->word;

	nword = malloc(sizeof *nword);
	char *word = nword->word;

	length = bonus ? bonusword(word, s) : getword(word, s);

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
 * Find the word that matches that has highest posy
 */
struct word *
searchstr(int key, char *str, int len, struct state *S)
{
	struct word *wordp = S->words;
	struct word *best = NULL;

	while (wordp) {
		if(
			wordp->length > len
			&& strncmp(wordp->word, str, len) == 0
			&& wordp->word[len] == key
			&& (!best || best->posy < wordp->posy)
		) {
			best = wordp;
		}
		wordp = wordp->nextword;
	}

	return best;
}


/*
 * look at the first character in each of the words to see if any match the
 * one that was typed.
 */
struct word *
searchchar(int key, struct state *S)
{
	struct word	*wordp, *best;

	for(best = NULL, wordp = S->words;
		wordp != NULL;
		wordp = wordp->nextword
	) {
		if(wordp->word[0] == key
 		&& (!best || best->posy < wordp->posy))
			best = wordp;
	}

	return best;
}

/* Delete the completed word and revise pointers  */
void
kill_word(struct word *wordp, struct state *S)
{
	struct word *temp, *prev = NULL;

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
		set_timer(NULL);
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


static void
set_handlers(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof act);
	act.sa_sigaction = assign_signal;
	if (sigaction(SIGALRM, &act, NULL)) {
		perror("sigaction");
		exit(1);
	}
}
