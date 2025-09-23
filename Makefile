#
# Makefile for letters, a game to help improve typing skills
#
# copyright 1991 by Larry Moss (lm03_cif@uhura.cc.rochester.edu)
#

#CC = cc
CC = gcc

SYSTYPE = SYSV
#SYSTYPE = NEXT
#SYSTYPE = BSD
#SYSTYPE = SYSV2

# if you don't have job control add -DNOJOB to CFLAGS.
CFLAGS = -g -O0 -D$(SYSTYPE) -DHIGHSCORES=\"$(LIBDIR)/letters.high\" \
	-DDICTIONARY=\"$(DICTIONARY)\"

LDFLAGS = -ltermcap -lcurses

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man/man$(MANEXT)
MANEXT = 6
LIBDIR = $(PREFIX)/lib
DICTIONARY = /usr/share/dict/words
#DICTIONARY = dictfile
INSTALL = /usr/bin/install

# next line only needed if you need to create a dictionary file.  The files
# in this directory will be used to make a wordlist.
DOCDIR = $(PREFIX)/man

OBJS = letters.o word.o highscore.o

# The following line will stop gcc from complaining about the arguments
# sun's make uses.  It shouldn't bother anyone else.
.c.o:
	$(CC) $(CFLAGS) -c $<

letters: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o letters $(LDFLAGS)

word.o highscore.o letters.o: \
		config.h

install: letters
	$(INSTALL) -s -m 2755 letters $(BINDIR)
	sed -e 's;LIBDIR;$(LIBDIR);' -e 's;DICTIONARY;$(DICTIONARY);'\
		letters.man > letters.$(MANEXT)
	$(INSTALL) -c -D -m 0644 letters.$(MANEXT) $(MANDIR)/letters.$(MANEXT)
	if [ ! -f $(LIBDIR)/letters.high ] ;then \
		$(INSTALL) -c -m 0664 letters.high $(LIBDIR) ;fi

install_hpux: letters
	install -c $(BINDIR) letters
	install -c $(MANDIR) letters.man
	install -c $(LIBDIR) letters.high
	chmod 0666 $(LIBDIR)/letters.high

clean:
	rm -f rm *.o letters letters.$(MANEXT)

shar:
	shar -o letters.shar *.[ch] Makefile* letters.man letters.high README \
		README.Amiga

tar:
	tar cf letters.tar *.[ch] Makefile* letters.man letters.high README \
		README.Amiga

dict:
	awk '{ for (i = 0; i <= NF; ++i)\
		if($$i ~ /^[a-zA-Z][a-z]*$$/) { print $$i } }'\
		`find $(DOCDIR) -type f -print` |\
		sort -u > $(DICTIONARY)
