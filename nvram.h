#ifndef NVRAM_H
#define NVRAM_H


/* Recognized hardware descriptions. */
#define HARDWARE_DESCRIPTION_STANDARD 0
#define HARDWARE_DESCRIPTION_INTEL    1
#define HARDWARE_DESCRIPTION_VIA82Cxx 2
#define HARDWARE_DESCRIPTION_VIA823x  3
#define HARDWARE_DESCRIPTION_DS1685   4

/* Hardware structure. */
typedef struct {
	int         type;
	const char *bios_vendor;
	const char *bios_version;
	const char *bios_release_date;
	const char *system_manufacturer;
	const char *system_productcode;
	const char *system_version;
	const char *board_manufacturer;
	const char *board_productcode;
	const char *board_version;
} hardware_t;

#endif
