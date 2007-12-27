/*
 *   map.c -- stores NVRAM mappings in a linked list.
 */

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "list.h"
#include "map.h"


/* Create a new NULL map field. */
map_field_t *map_field_new(void)
{
	map_field_t *map_field;

	/* Get memory for map field. */
	if ((map_field=malloc(sizeof(map_field_t))) == NULL) return NULL;

	/* Initialize map field. */
	INIT_LIST_HEAD(&map_field->list);
	map_field->type=MAP_FIELD_TYPE_NULL;

	return map_field;
}


/* Destroy (unlink & free) a single map field. */
void map_field_destroy(map_field_t *ptr)
{
	list_del(&ptr->list);
	free(ptr);
}


/* Destroy (free all) a list of map fields. */
void map_field_destroy_list(map_field_t *ptr)
{
	struct list_head *pos, *temp;

	list_for_each_safe(pos, temp, &(ptr->list)) { 
		free(list_entry(pos, map_field_t, list));
	}	
}
