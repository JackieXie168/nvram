#ifndef NVRAM_OP_H
#define NVRAM_OP_H


/* NVRAM chip types. */
#define NVRAM_TYPE_UNKNOWN   -1
#define NVRAM_TYPE_INTEL      0
#define NVRAM_TYPE_VIA82Cxx   1
#define NVRAM_TYPE_VIA823x    2
#define NVRAM_TYPE_DS1685     3

/* NVRAM operations. */
int nvram_open(int nvram_type_arg);
int nvram_close(void);
unsigned char nvram_read(unsigned int address);
void nvram_write(unsigned int address, unsigned char data);

#endif
