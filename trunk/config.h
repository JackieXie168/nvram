#ifndef CONFIG_H
#define CONFIG_H

#include "detect.h"
#include "list.h"


/* Config files. */
#define CONFIG_BASE_FILENAME "./nvram.conf"
#define CONFIG_PATH_LENGTH_MAX 1000
#define CONFIG_NESTING_MAX 100

/* Config operations. */
void read_config(struct list_head *token_list, hardware_t *hardware_description, struct list_head *nvram_mapping);

#endif
