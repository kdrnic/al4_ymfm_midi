#include <string.h>

#include "crc32.h"

void crc32_init(unsigned int *state)
{
	const crc32_state_t s = CRC32_STATE0;
	memcpy(state, s, sizeof(s));
}

unsigned int crc32_buffer(char *message, int len, unsigned int *state)
{
	int i, j;
	unsigned int crc = state[0], mask = state[1];
	
	for(i = 0; i < len; i++){
		const unsigned int byte = ((unsigned char *) message)[i];
		crc = crc ^ byte;
		for(j = 7; j >= 0; j--){
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}
	
	state[0] = crc;
	state[1] = mask;
	
	return ~crc;
}

unsigned int crc32_string(char *message, unsigned int *state)
{
	int i, j;
	unsigned int crc = state[0], mask = state[1];
	
	for(i = 0; message[i]; i++){
		const unsigned int byte = message[i];
		crc = crc ^ byte;
		for(j = 7; j >= 0; j--){
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}
	
	state[0] = crc;
	state[1] = mask;
	
	return ~crc;
}
