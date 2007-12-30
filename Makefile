.PHONY: all clean install distclean mrproper

CC=gcc
CFLAGS+=-std=c99 -Wall -pedantic -DHAS_LOCALE
LD=gcc
LDFLAGS+=


all: nvram

clean:
	-rm *.o nvram

install: all distclean
	install nvram $(PREFIX)/sbin
	install nvram.conf /etc
	install -d /etc/nvram.d
	install conf.d/atbios /etc/nvram.d
	install conf.d/via823x /etc/nvram.d
	-ln -s via823x '/etc/nvram.d/VIA Technologies, Inc.:VT8367-8233'

distclean:
	-rm $(PREFIX)/sbin/nvram

mrproper: distclean
	-rm -rf /etc/nvram.conf /etc/nvram.d


nvram: main.o config.o util.o token.o map.o nvram_op.o detect.o
	$(LD) $(LDFLAGS) -o $@ $^
