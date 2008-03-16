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
	install -d /etc/nvram.d/hardware
	install conf.d/ds1685   /etc/nvram.d/hardware
	install conf.d/intel    /etc/nvram.d/hardware
	install conf.d/via82cxx /etc/nvram.d/hardware
	install conf.d/via823x  /etc/nvram.d/hardware
	-ln -s intel    '/etc/nvram.d/hardware/ASUSTeK Computer Inc.:P4C800'
	-ln -s intel    '/etc/nvram.d/hardware/ASUSTeK Computer Inc.:P4C800-E'
	-ln -s intel    '/etc/nvram.d/hardware/ASUSTeK Computer Inc.:P4P800'
	-ln -s intel    '/etc/nvram.d/hardware/ASUSTeK Computer INC.:P4P800-VM'
	-ln -s intel    '/etc/nvram.d/hardware/INTEL:SpringDale-G'
	-ln -s intel    '/etc/nvram.d/hardware/MICRO-STAR INC.:MS-6580'
	-ln -s via823x  '/etc/nvram.d/hardware/ASUS:A7V8X-MX SE'
	-ln -s via823x  '/etc/nvram.d/hardware/ASUSTek Computer Inc.:K8V'
	-ln -s via823x  '/etc/nvram.d/hardware/ASUSTeK Computer Inc.:K8V'
	-ln -s via823x  '/etc/nvram.d/hardware/ASUSTeK Computer Inc.:K8VSEDX'
	-ln -s via823x  '/etc/nvram.d/hardware/ECS Elitegroup:L7VMM2'
	-ln -s via823x  '/etc/nvram.d/hardware/Gigabyte Technology Co., Ltd.:GA-7VT600 1394'
	-ln -s via823x  '/etc/nvram.d/hardware/MICRO-STAR INTERNATIONAL CO., LTD:MS-6734'
	-ln -s via823x  '/etc/nvram.d/hardware/Shuttle Inc:Fx41'
	-ln -s via823x  '/etc/nvram.d/hardware/Shuttle Inc:SK43G'
	-ln -s via823x  '/etc/nvram.d/hardware/VIA Technologies, Inc.:KT400-8235'
	-ln -s via823x  '/etc/nvram.d/hardware/VIA Technologies, Inc.:VT8367-8233'
	-ln -s via823x  '/etc/nvram.d/hardware/VIA Technologies, Inc.:VT8367-8235'
	-ln -s via82cxx '/etc/nvram.d/hardware/ASUSTeK Computer INC.:A7NVM400'
	-ln -s via82cxx '/etc/nvram.d/hardware/ASUSTeK Computer INC.:P4R8L'

distclean:
	-rm $(PREFIX)/sbin/nvram

mrproper: distclean
	-rm -rf /etc/nvram.conf /etc/nvram.d


nvram: main.o config.o util.o token.o map.o nvram_op.o detect.o
	$(LD) $(LDFLAGS) -o $@ $^
