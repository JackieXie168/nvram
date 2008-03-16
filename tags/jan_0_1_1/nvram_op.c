/*
 *   nvram_op.c -- basic operations on the nvram.
 *
 *   Copyright (c) 2007, Jan Kandziora <nvram@kandziora-ing.de>
 * 
 */

#include <string.h>
#include <sys/io.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <wchar.h>

#include "nvram.h"
#include "nvram_op.h"


/* Global variable which stores the NVRAM type. */
int nvram_type;

/* Global variable which stores the value of RTC register A. Important for DS1685 hardware type. */
int nvram_register_a;


/* NVRAM cache */
struct {
	unsigned char value;
	char          valid;
	char          written;
	char          flushed;
} nvram_cache[NVRAM_SIZE];


/* Get access to nvram. */
int nvram_open(int nvram_type_arg)
{
	/* Get permissions to access the nvram. */
	if (ioperm(0x70,6,1) == -1) return -1;

	/* Set nvram type. */
	nvram_type=nvram_type_arg;

	/* Get initial contents of RTC register A. */
	outb(0x0a,0x70);
	nvram_register_a=inb(0x71);

	/* Initialize NVRAM cache. */
	memset(&nvram_cache, 0, sizeof(nvram_cache));

	/* Initialization OK. */
	return 0;
}


/* Release access to nvram. */
int nvram_close(void)
{
	/* Switch bank to 0 if neccessary. */
	if ((nvram_register_a & 0x10)) {
		nvram_register_a &= 0xef;
		outb(0x0a,0x70);
		outb(nvram_register_a,0x71);
	}

	/* Release permissions to access the nvram. */
	return (ioperm(0x70,6,0));
}


/* Address a byte in the nvram. Returns the address of the data register. */
static int nvram_address(unsigned int address)
{
	/* Return error on addresses >=256. */
	if (address >= NVRAM_SIZE) return -1;

	/* Different handling of addresses <128 and above. */
	if (address < 128) {
		switch (nvram_type) {
 			case HARDWARE_TYPE_DS1685:
				/* Dallas DS1685 uses bit 4 of special register A in the RTC to switch banks. */
				/* Switch bank to 0 if neccessary. */
        if ((nvram_register_a & 0x10)) {
					nvram_register_a &= 0xef;
					outb(0x0a,0x70);
					outb(nvram_register_a,0x71);
				}
        outb(address,0x70);
        return 0x71;
			default:	
				/* Adresses below 128 can be accessed via the usual port 0x70/0x71 mechanism on all other chips. */
				outb(address,0x70);
				return 0x71;
		}
	} else {
		switch (nvram_type) {
			case HARDWARE_TYPE_INTEL:
				/* Intel just added another register set 0x72/0x73 for the second 128 bytes. */
				outb(address-128,0x72);
				return 0x73;
			case HARDWARE_TYPE_VIA82Cxx:
				/* VIA in the 82Cxxx southbridges just did as intel, but needs to have bit 7 of 0x72 set to 1, too. */
				outb(address,0x72);
				return 0x73;
			case HARDWARE_TYPE_VIA823x:
				/* VIA in the 823x southbridges just did as before, but with 0x74/0x75 port. */
				outb(address,0x74);
				return 0x75;
 			case HARDWARE_TYPE_DS1685:
				/* Dallas uses bit 4 of special register 0xa in the RTC to switch banks. */
				/* Switch bank to 1 if neccessary. */
        if (!(nvram_register_a & 0x10)) {
					nvram_register_a |= 0x10;
					outb(0x0a,0x70);
					outb(nvram_register_a,0x71);
				}

				/* Dallas uses the special register 0x50/0x53 of bank 1 in the RTC as the extended address/data register. */
				outb(0x50,0x70);
				outb(address-128,0x71);
				outb(0x53,0x70);
        return 0x71;
			default:
				/* By default, upper nvram doesn't exist. Return -1 in that case. */
				return -1;
		}
	}
}


/* Read a byte of nvram. */
unsigned char nvram_read(unsigned int address)
{
	unsigned int  data_register;
	unsigned char data;
	
	/* Return 0xff for invalid addresses. */
	if (address >= NVRAM_SIZE) return 0xff;

	/* Return cached byte, if any. */
	if (nvram_cache[address].valid) return nvram_cache[address].value;

	/* Address the byte to read. */
	/* Return 0xff for nonexisting addresses. */
	if ((data_register=nvram_address(address)) == -1) return 0xff;

	/* Read the byte. */
	data=inb(data_register);

	/* Update the cache. */
	nvram_cache[address].value=data;
	nvram_cache[address].valid=1;

	/* Return the data. */
	return data;
}


/* Write a byte to NVRAM cache. */
void nvram_write(unsigned int address, unsigned char data)
{
	/* Ignore invalid addresses. */
	if (address >= NVRAM_SIZE) return;

	/* Update cache. */
	nvram_cache[address].value=data;
	nvram_cache[address].valid=1;
	nvram_cache[address].written=1;
	nvram_cache[address].flushed=0;
}


/* Write NVRAM cache back to NVRAM. */
void nvram_flush(void)
{
	unsigned int data_register;
	unsigned int i;

	/* Go through all cache addresses. */
	for (i=0; i < NVRAM_SIZE; i++) {
		/* Check if we have to flush that specific byte. */
		if (nvram_cache[i].valid && nvram_cache[i].written && !(nvram_cache[i].flushed)) {
			/* Address the byte to write. */
			/* Do nothing for nonexisting addresses. */
			if ((data_register=nvram_address(i)) != -1) {
				/* Write the byte into NVRAM. */
				outb(nvram_cache[i].value, data_register);

				/* Mark byte as flushed. */
				nvram_cache[i].flushed=1;
			}	
		}	
	}
}

