#include <allegro.h>
#include "wav.h"

int save_wav(const char *filename, SAMPLE *spl)
{
	int bps = spl->bits/8 * ((spl->stereo) ? 2 : 1);
	int len = spl->len * bps;
	int i;
	signed short s;
	PACKFILE *f;
	
	f = pack_fopen(filename, F_WRITE);
	
	if(f){
		pack_fputs("RIFF", f);                 /* RIFF header */
		pack_iputl(36+len, f);                 /* size of RIFF chunk */
		pack_fputs("WAVE", f);                 /* WAV definition */
		pack_fputs("fmt ", f);                 /* format chunk */
		pack_iputl(16, f);                     /* size of format chunk */
		pack_iputw(1, f);                      /* PCM data */
		pack_iputw((spl->stereo) ? 2 : 1, f);  /* mono/stereo data */
		pack_iputl(spl->freq, f);              /* sample frequency */
		pack_iputl(spl->freq*bps, f);          /* avg. bytes per sec */
		pack_iputw(bps, f);                    /* block alignment */
		pack_iputw(spl->bits, f);              /* bits per sample */
		pack_fputs("data", f);                 /* data chunk */
		pack_iputl(len, f);                    /* actual data length */
		
		if(spl->bits == 8){
			pack_fwrite(spl->data, len, f);     /* write the data */
		}
		else{
			for(i=0; i < (int) spl->len * ((spl->stereo) ? 2 : 1); i++){
				s = ((signed short *) spl->data)[i];
				pack_iputw(s ^ 0x8000, f);
			}
		}
		
		pack_fclose(f);
	}
	
	return errno;
}

