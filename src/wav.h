#ifndef WAV_H
#define WAV_H

struct SAMPLE;

#ifdef __cplusplus
extern "C"{
#endif

int save_wav(const char *filename, SAMPLE *spl);

#ifdef __cplusplus
}
#endif

#endif
