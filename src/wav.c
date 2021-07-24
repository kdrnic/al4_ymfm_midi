#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "wav.h"

/* pack_iputw:
 *  Writes a 16 bit int to a file, using intel byte ordering.
 */
static void pack_iputw(int w, FILE *f)
{
	int b1, b2;
	b1 = (w & 0xFF00) >> 8;
	b2 = w & 0x00FF;
	     putc(b2,f);
	     putc(b1,f);
}

/* pack_iputl:
 *  Writes a 32 bit long to a file, using intel byte ordering.
 */
static void pack_iputl(long l, FILE *f)
{
	int b1, b2, b3, b4;
	b1 = (int)((l & 0xFF000000L) >> 24);
	b2 = (int)((l & 0x00FF0000L) >> 16);
	b3 = (int)((l & 0x0000FF00L) >> 8);
	b4 = (int)l & 0x00FF;
	     putc(b4,f);
	     putc(b3,f);
	     putc(b2,f);
	     putc(b1,f);
}

KDR_SAMPLE *kdr_create_sample(int bits, int stereo, int freq, int len)
{
	KDR_SAMPLE *spl;
	assert(freq > 0);
	assert(len > 0);
	
	spl = malloc(sizeof(KDR_SAMPLE)); 
	if (!spl)
		return NULL;
	
	spl->bits = bits;
	spl->stereo = stereo;
	spl->freq = freq;
	spl->priority = 128;
	spl->len = len;
	spl->loop_start = 0;
	spl->loop_end = len;
	spl->param = 0;
	
	spl->data = malloc(len * ((bits==8) ? 1 : sizeof(short)) * ((stereo) ? 2 : 1));
	if (!spl->data) {
		free(spl);
		return NULL;
	}
	
	return spl;
}

void kdr_destroy_sample(KDR_SAMPLE *spl)
{
	if(spl){
		if(spl->data){
			free(spl->data);
		}
		free(spl);
   }
}

int kdr_save_wav(const char *filename, KDR_SAMPLE *spl)
{
	int bps = spl->bits/8 * ((spl->stereo) ? 2 : 1);
	int len = spl->len * bps;
	int i;
	signed short s;
	FILE *f;
	
	f = fopen(filename, "wb");
	
	if(f){
		     fputs("RIFF", f);                 /* RIFF header */
		pack_iputl(36+len, f);                 /* size of RIFF chunk */
		     fputs("WAVE", f);                 /* WAV definition */
		     fputs("fmt ", f);                 /* format chunk */
		pack_iputl(16, f);                     /* size of format chunk */
		pack_iputw(1, f);                      /* PCM data */
		pack_iputw((spl->stereo) ? 2 : 1, f);  /* mono/stereo data */
		pack_iputl(spl->freq, f);              /* sample frequency */
		pack_iputl(spl->freq*bps, f);          /* avg. bytes per sec */
		pack_iputw(bps, f);                    /* block alignment */
		pack_iputw(spl->bits, f);              /* bits per sample */
		     fputs("data", f);                 /* data chunk */
		pack_iputl(len, f);                    /* actual data length */
		
		if(spl->bits == 8){
			     fwrite(spl->data, 1, len, f);     /* write the data */
		}
		else{
			for(i=0; i < (int) spl->len * ((spl->stereo) ? 2 : 1); i++){
				s = ((signed short *) spl->data)[i];
				pack_iputw(s ^ 0x8000, f);
			}
		}
		
		     fclose(f);
	}
	
	return 0;
}

