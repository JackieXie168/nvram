.PHONY: all clean install distclean

CC=gcc
CFLAGS+=-std=c99 -Wall -pedantic -DHAS_LOCALE
LD=gcc
LDFLAGS+=-static


all: nvram

clean:
	-rm *.o nvram

install: distclean
	install nvram $(PREFIX)/bin

distclean:
	-rm $(PREFIX)/bin/nvram


nvram: main.o util.o token.o map.o nvram_op.o
	$(LD) $(LDFLAGS) -o $@ $^
	
