#ifndef CRC32
#define CRC32

typedef unsigned int crc32_state_t[2];
#define CRC32_STATE_LEN
#define CRC32_STATE0 {0xFFFFFFFF, 0}

void crc32_init(unsigned int *state);
unsigned int crc32_buffer(char *message, int len, unsigned int *state);
unsigned int crc32_string(char *message, unsigned int *state);

#endif
