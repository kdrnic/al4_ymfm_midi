#ifndef WAV_H
#define WAV_H

#ifdef __cplusplus
extern "C"{
#endif

typedef struct KDR_SAMPLE               /* a sample */
{
	int bits;                           /* 8 or 16 */
	int stereo;                         /* sample type flag */
	int freq;                           /* sample frequency */
	int priority;                       /* 0-255 */
	unsigned long len;                  /* length (in samples) */
	unsigned long loop_start;           /* loop start position */
	unsigned long loop_end;             /* loop finish position */
	unsigned long param;                /* for internal use by the driver */
	void *data;                         /* sample data */
} KDR_SAMPLE;

void kdr_destroy_sample(KDR_SAMPLE *spl);
KDR_SAMPLE *kdr_create_sample(int bits, int stereo, int freq, int len);
int kdr_save_wav(const char *filename, KDR_SAMPLE *spl);

#ifdef __cplusplus
}
#endif

#endif
