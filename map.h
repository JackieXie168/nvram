#ifndef MAP_H
#define MAP_H

#include "list.h"


/* Maximum number of values per bitfield. */
#define MAP_BITFIELD_MAX_BITS 5   

/* Field types. */
#define MAP_FIELD_TYPE_NULL      0
#define MAP_FIELD_TYPE_BYTEARRAY 1
#define MAP_FIELD_TYPE_STRING    2
#define MAP_FIELD_TYPE_BITFIELD  3

typedef struct {
	struct list_head  list;
	unsigned int      type;
	wchar_t          *name;
	union {
		struct {
			unsigned int position;
			unsigned int length;
		} bytearray;
		struct {
			unsigned int position;
			unsigned int length;
		} string;
		struct {
			unsigned char length;
			struct {
				unsigned int  byte;
				unsigned char bit;
			} position[MAP_BITFIELD_MAX_BITS];
			wchar_t *values[1<<MAP_BITFIELD_MAX_BITS];
		} bitfield;
	} data;	
} map_field_t;


/* Mapping operations. */
map_field_t *map_field_new(void);
void map_field_destroy(map_field_t *ptr);
void map_field_destroy_list(map_field_t *ptr);

#endif
