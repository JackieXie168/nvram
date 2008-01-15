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

/* Loglevels. */
#define LOGLEVEL_DEBUG   0
#define LOGLEVEL_INFO    1
#define LOGLEVEL_WARNING 2
#define LOGLEVEL_ERROR   3

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
	int   type;
	char *bios_vendor;
	char *bios_version;
	char *bios_release_date;
	char *system_manufacturer;
	char *system_productcode;
	char *system_version;
	char *board_manufacturer;
	char *board_productcode;
	char *board_version;
} hardware_t;

/* Settings given in the configuration and/or command line. */
typedef struct {
	int    argc;
	char **argv;
	char   dmi_raw;
	char   loglevel;
	char   update_checksums;
	char   write_to_nvram;
} settings_t;

#endif
