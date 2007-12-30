#ifndef CONFIG_H
#define CONFIG_H

#include "detect.h"
#include "list.h"
#include "nvram.h"


/* Config operations. */
void read_config(struct list_head *token_list, hardware_t *hardware_description, struct list_head *nvram_mapping);

#endif
