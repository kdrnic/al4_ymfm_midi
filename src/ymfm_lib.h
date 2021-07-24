#ifndef YMFM_LIB_H

//Magic number from the internets
#define YMFM_SB16_OPL_CLOCK_RATE 14318181

#define YMFMLIB_USE_LIBRESAMPLE  1

#ifdef __cplusplus
extern "C" {
#endif
int ymfm_is_stereo(void *dv);
int ymfm_get_sampling_rate(void *dv);
void *ymfm_init(unsigned int clock, unsigned int output_rate_, int stereo_);
void ymfm_openlog(void *dv, const char *fn);
void ymfm_write(void *dv, int reg, unsigned char data);
void ymfm_generate(void *dv, void *buffer, int num_samples);
void ymfm_destroy(void *dv);
#ifdef __cplusplus
}
#endif

#endif
