#ifndef NVRAM_H
#define NVRAM_H


/* Config files. */
#define CONFIG_DIRECTORY "/etc/nvram.d"
#define CONFIG_BASE_FILENAME "/etc/nvram.conf"
#define CONFIG_PATH_LENGTH_MAX 1000
#define CONFIG_NESTING_MAX 100

/* Checksum algorithms. */
#define CHECKSUM_ALGORITHM_STANDARD_SUM       0
#define CHECKSUM_ALGORITHM_STANDARD_SHORT_SUM 1
#define CHECKSUM_ALGORITHM_NEGATIVE_SUM       2
#define CHECKSUM_ALGORITHM_NEGATIVE_SHORT_SUM 3

/* Maximum NVRAM size. */
#define NVRAM_SIZE 256

/* Hardware types. */
#define HARDWARE_TYPE_STANDARD 0
#define HARDWARE_TYPE_INTEL    1
#define HARDWARE_TYPE_VIA82Cxx 2
#define HARDWARE_TYPE_VIA823x  3
#define HARDWARE_TYPE_DS1685   4

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

/* Settings given in the configuration and/or command line. */
typedef struct {
	int    argc;
	char **argv;
	char   write_to_nvram;
	char   update_checksums;
	char   verbose;
} settings_t;

#endif
