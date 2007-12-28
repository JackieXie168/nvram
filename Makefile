.PHONY: all clean install distclean

CC=gcc
CFLAGS+=-std=c99 -Wall -pedantic -DHAS_LOCALE
LD=gcc
LDFLAGS+=


all: nvram

clean:
	-rm *.o nvram

install: distclean
	install nvram $(PREFIX)/bin

distclean:
	-rm $(PREFIX)/bin/nvram


nvram: main.o config.o util.o token.o map.o nvram_op.o detect.o
	$(LD) $(LDFLAGS) -o $@ $^
	
