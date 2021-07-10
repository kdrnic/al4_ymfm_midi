#ifndef YMFM_LIB_H

//Magic number from the internets
#define SB16_OPL_CLOCK_RATE 14318181

#define YMFMLIB_RESAMPLE        1
#define YMFMLIB_TIME_REG_WRITES 1
#define YMFMLIB_USE_LIBRESAMPLE 1

#ifdef __cplusplus
extern "C" {
#endif
void ymfm_init(unsigned int clock, unsigned int output_rate, int stereo);
void ymfm_write(int reg, unsigned char data);
void ymfm_generate(void *buffer, int num_samples);
#ifdef __cplusplus
}
#endif

#endif
