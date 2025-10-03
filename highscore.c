/*
 * read and update high score file for Letter Invaders
 *
 * copyright 1991 by Larry Moss (lm03_cif@uhura.cc.rochester.edu)
 * copyright 2025 William Pursell (william.r.pursell@gmail.com)
 */

/*
 * TODO
 * SIGWINCH and different sized screens make the high scores irrelevant
 * A player on a large screen has a huge advantage over a player with
 * fewer LINES.  Maybe track different high score files for different
 * screen sizes, and probably invalidate the game if SIGWINCH is ever
 * received.  Yes, that seems like a good idea.  Track the max value
 * of LINES over the course of the game and use the high-score list
 * for that screen size.  It's not clear how COLS impacts things. A tight
 * screen might be easier, so maybe track both.
 */


#include "letters.h"

#define SCORE_FMT "%5d %6d %6d"

static struct score_rec high_scores[10];
static struct stat	s_buf;
static time_t readtime;

char *score_header = "    name       level  words  score";

int
read_scores(void)
{
	char *highscores = HIGHSCORES;
	struct score_rec *h = high_scores;
	FILE		 *fp;

	/*
	 * get the last modified time so we know later if we have to reread
	 * the scores before saving a new file.  Only important on
	 * multiuser systems.
	 */
	if(
		stat(highscores, &s_buf) == -1 ||
		(fp = fopen(highscores, "r")) == NULL
	) {
		perror(highscores);
		return 1;
	}

	readtime = s_buf.st_mtime;

	while ( h - high_scores < 10 && 4 == fscanf(
		fp, "%8s %d %d %d", h->name, &h->level, &h->words, &h->score
	)) {
		h += 1;
	}

	fclose(fp);
	return 0;
}

int
write_scores() {
	int	i;
	FILE	*fp;
	char *highscores = HIGHSCORES;

	/*
	 * check to make sure the high score list has not been modified
	 * since we read it.
	 *
	 * TODO Fix race with file locking
	 */
	if( stat(highscores, &s_buf) == -1 ) {
		endwin();
		perror(highscores);
		exit(1);
	}

	if(s_buf.st_mtime > readtime)
		return -1;

	if((fp = fopen(highscores, "w")) == NULL) {
		endwin();
		perror(highscores);
		exit(1);
	}

	for (i = 0; i < 10; i += 1) {
		struct score_rec *h = high_scores + i;
		fprintf(fp, "%s %d %d %d\n",
			h->name,
			h->level,
			h->words,
			h->score
		);
	}

	return fclose(fp);
}


char *
username(void)
{
	struct passwd *p;
	if((p = getpwuid(getuid())) == NULL) {
		return "nobody";
	} else {
		return p->pw_name;
	}
}


void
update_scores(struct score *score, unsigned level)
{
	int i, j;

	read_scores();
	for (i = 0; i < 10; i += 1) {
		struct score_rec *h = high_scores + i;
		if (score->points > h->score) {
			if (i < 9) {
				memmove(h + 1, h, (9 - i) * sizeof *h);
			}
			strcpy(h->name, username());
			h->score = score->points;
			h->words = score->words;
			h->level = level;
			if (write_scores() == -1) {
				read_scores();
				update_scores(score, level);
			}
			break;
		}
	}
}


/* Iterate through the high scores, writing a printable string to buf */
struct score_rec *
next_score(char *buf, size_t siz)
{
	static int idx = 0;

	if (idx == sizeof high_scores / sizeof *high_scores) {
		idx = 0;
		return NULL;
	}

	if (idx == 0) {
		read_scores();
	}

	struct score_rec *h = high_scores + idx;
	snprintf(buf, siz, "%3d %-11s" SCORE_FMT,
		idx + 1,
		h->name,
		h->level,
		h->words,
		h->score
	);

	idx += 1;

	return h;
}


void
show_scores(struct state *S)
{
	struct score_rec *h;
	char *header = "Top Ten Scores for Letter Invaders";
	int x = (COLS - strlen(header)) / 2;
	int y = (LINES - 12) / 3;
	int on_board = 0;

	read_scores();

	erase();
	attron(A_STANDOUT);
	mvaddstr(y, x, header);
	attroff(A_STANDOUT);
	attron(A_UNDERLINE);
	mvaddstr(y += 2, x, score_header);
	attroff(A_UNDERLINE);

	for (char s[64]; NULL != (h = next_score(s, sizeof s)); ) {
		if (
			! on_board &&
			h->score == S->score.points &&
			!strcmp(h->name, username()))
		{
			attron(A_STANDOUT);
			on_board = 1;
		}
		mvaddstr(++y, x, s);
		attroff(A_STANDOUT);
	}
	if (!on_board) {
		attron(A_STANDOUT);
		mvprintw(y += 2, x, ">10 ");
		printw("%-10s " SCORE_FMT,
			username(),
			S->level,
			S->score.words,
			S->score.points
		);
	}
	attroff(A_STANDOUT);

	refresh();

	mvaddstr(y += 2, x + 2, "Press any key to continue");
	getch();
}
