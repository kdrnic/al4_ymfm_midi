//From DOS version of Allegro, modified to use YMFM

/*         ______   ___    ___ 
 *        /\  _  \ /\_ \  /\_ \ 
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___ 
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      Adlib/FM driver for the MIDI player.
 *
 *      By Shawn Hargreaves.
 *
 *      See readme.txt for copyright information.
 */


#include <string.h>

//#include <allegro.h>

#define KDR_INTERNAL
#include "kdr_midi.h"
//#include "allegro/platform/aintdos.h"

//#ifndef ALLEGRO_DOS
//   #error something is wrong with the makefile
//#endif

#include "ymfm_lib.h"

#define MIDI_OPL2   KDR_MIDI_OPL2   //         AL_ID('O','P','L','2')
#define MIDI_2XOPL2 KDR_MIDI_2XOPL2 //         AL_ID('O','P','L','X')
#define MIDI_OPL3   KDR_MIDI_OPL3   //         AL_ID('O','P','L','3')

/* external interface to the Adlib driver */
static int fm_detect(struct KDR_MIDI_CTX *ctx, int input);
static int fm_init(struct KDR_MIDI_CTX *ctx, int input, int voices, void *param);
static void fm_exit(struct KDR_MIDI_CTX *ctx, int input);
static int fm_set_mixer_volume(struct KDR_MIDI_CTX *ctx, int volume);
static int fm_load_patches(struct KDR_MIDI_CTX *ctx, AL_CONST char *patches, AL_CONST char *drums);
static void fm_key_on(struct KDR_MIDI_CTX *ctx, int inst, int note, int bend, int vol, int pan);
static void fm_key_off(struct KDR_MIDI_CTX *ctx, int voice);
static void fm_set_volume(struct KDR_MIDI_CTX *ctx, int voice, int vol);
static void fm_set_pitch(struct KDR_MIDI_CTX *ctx, int voice, int note, int bend);

const MIDI_DRIVER kdr_midi_opl2 =
{
   .id               = MIDI_OPL2,
   .name             = EMPTY_STRING, 
   .desc             = EMPTY_STRING,
   .ascii_name       = "Adlib (OPL2)",
   .voices           = 9,
   .basevoice        = 0, 
   .max_voices       = 9, 
   .def_voices       = 9, 
   .detect           = fm_detect,
   .init             = fm_init,
   .exit             = fm_exit,
   .set_mixer_volume = fm_set_mixer_volume,
   .get_mixer_volume = NULL,
   .raw_midi         = NULL,
   .load_patches     = fm_load_patches,
   .adjust_patches   = _dummy_adjust_patches,
   .key_on           = fm_key_on,
   .key_off          = fm_key_off,
   .set_volume       = fm_set_volume,
   .set_pitch        = fm_set_pitch,
   .set_pan          = _dummy_noop2,
   .set_vibrato      = _dummy_noop2
};



const MIDI_DRIVER kdr_midi_2xopl2 =
{
   .id               = MIDI_2XOPL2,
   .name             = EMPTY_STRING, 
   .desc             = EMPTY_STRING,
   .ascii_name       = "Adlib (dual OPL2)",
   .voices           = 18, 
   .basevoice        = 0, 
   .max_voices       = 18,
   .def_voices       = 18, 
   .detect           = fm_detect,
   .init             = fm_init,
   .exit             = fm_exit,
   .set_mixer_volume = fm_set_mixer_volume,
   .get_mixer_volume = NULL,
   .raw_midi         = NULL,
   .load_patches     = fm_load_patches,
   .adjust_patches   = _dummy_adjust_patches,
   .key_on           = fm_key_on,
   .key_off          = fm_key_off,
   .set_volume       = fm_set_volume,
   .set_pitch        = fm_set_pitch,
   .set_pan          = _dummy_noop2,
   .set_vibrato      = _dummy_noop2
};



const MIDI_DRIVER kdr_midi_opl3 =
{
   .id               = MIDI_OPL3,
   .name             = EMPTY_STRING, 
   .desc             = EMPTY_STRING,
   .ascii_name       = "Adlib (OPL3)",
   .voices           = 18,
   .basevoice        = 0, 
   .max_voices       = 18, 
   .def_voices       = 18,
   .detect           = fm_detect,
   .init             = fm_init,
   .exit             = fm_exit,
   .set_mixer_volume = fm_set_mixer_volume,
   .get_mixer_volume = NULL,
   .raw_midi         = NULL,
   .load_patches     = fm_load_patches,
   .adjust_patches   = _dummy_adjust_patches,
   .key_on           = fm_key_on,
   .key_off          = fm_key_off,
   .set_volume       = fm_set_volume,
   .set_pitch        = fm_set_pitch,
   .set_pan          = _dummy_noop2,
   .set_vibrato      = _dummy_noop2
};



#define FM_HH     1
#define FM_CY     2
#define FM_TT     4
#define FM_SD     8
#define FM_BD     16


/* include the GM patch set (static data) */
#include "fm_instr.h"
#include "fm_drum.h"

//Made from non-const globals from adlib.c
typedef struct fm_driver_data
{
	/* OPL emulator */
	void *ymfm;
	
	FM_INSTRUMENT fm_drum[FM_DRUM_NUM];
	FM_INSTRUMENT fm_instrument[FM_INSTRUMENT_NUM];

	/* is the OPL in percussion mode? */
	int fm_drum_mode;

	/* cached information about the state of the drum channels */
	FM_INSTRUMENT *fm_drum_cached_inst1[5];
	FM_INSTRUMENT *fm_drum_cached_inst2[5];
	int fm_drum_cached_vol1[5];
	int fm_drum_cached_vol2[5];
	long fm_drum_cached_time[5];

	/* various bits of information about the current state of the FM chip */
	unsigned char fm_drum_mask;
	unsigned char fm_key[18];
	unsigned char fm_keyscale[18];
	unsigned char fm_feedback[18];
	int fm_level[18];
	int fm_patch[18];
} fm_driver_data;

#define FMDD ((struct fm_driver_data *)ctx->driver_data)

/* register offsets for each voice */
static const int fm_offset[18] = {
   0x000, 0x001, 0x002, 0x008, 0x009, 0x00A, 0x010, 0x011, 0x012, 
   0x100, 0x101, 0x102, 0x108, 0x109, 0x10A, 0x110, 0x111, 0x112
};

/* for converting midi note numbers to FM frequencies */
static const int fm_freq[13] = {
   0x157, 0x16B, 0x181, 0x198, 0x1B0, 0x1CA,
   0x1E5, 0x202, 0x220, 0x241, 0x263, 0x287, 0x2AE
};

/* logarithmic relationship between midi and FM volumes */
static const int fm_vol_table[128] = {
   0,  11, 16, 19, 22, 25, 27, 29, 32, 33, 35, 37, 39, 40, 42, 43,
   45, 46, 48, 49, 50, 51, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
   64, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 75, 76, 77,
   78, 79, 80, 80, 81, 82, 83, 83, 84, 85, 86, 86, 87, 88, 89, 89,
   90, 91, 91, 92, 93, 93, 94, 95, 96, 96, 97, 97, 98, 99, 99, 100,
   101, 101, 102, 103, 103, 104, 104, 105, 106, 106, 107, 107, 108,
   109, 109, 110, 110, 111, 112, 112, 113, 113, 114, 114, 115, 115,
   116, 117, 117, 118, 118, 119, 119, 120, 120, 121, 121, 122, 122,
   123, 123, 124, 124, 125, 125, 126, 126, 127
};

/* drum channel tables:          BD       SD       TT       CY       HH    */
static const int fm_drum_channel[] = { 6,       7,       8,       8,       7     };
static const int fm_drum_op1[] =     { TRUE,    FALSE,   TRUE,    FALSE,   TRUE  };
static const int fm_drum_op2[] =     { TRUE,    TRUE,    FALSE,   TRUE,    FALSE };

#define VOICE_OFFSET(x)     ((x < 9) ? x : 0x100+x-9)

/* fm_write:
 *  Writes a byte to the specified register on the FM chip.
 */
//MODIFIED TO USE ymfm RATHER THAN MS-DOS outportb
static void fm_write(void *ymfm, int reg, unsigned char data)
{	
   assert(reg >= 0 && reg <= 0x1FF);
   
   ymfm_write(ymfm, reg, data);
}

END_OF_STATIC_FUNCTION(fm_write);

/* fm_reset:
 *  Resets the FM chip. If enable is set, puts OPL3 cards into OPL3 mode,
 *  otherwise puts them into OPL2 emulation mode.
 */
static void fm_reset(struct KDR_MIDI_CTX *ctx, int enable)
{
   int i;
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;

   for (i=0xF5; i>0; i--)
      fm_write(FMDD->ymfm, i, 0);

   if (ctx->midi_card == MIDI_OPL3) {          /* if we have an OPL3... */

      fm_write(FMDD->ymfm, 0x105, 1);                 /* enable OPL3 mode */

      for (i=0x1F5; i>0x105; i--)
	 fm_write(FMDD->ymfm, i, 0);

      for (i=0x104; i>0x100; i--)
	 fm_write(FMDD->ymfm, i, 0);

      if (!enable)
	 fm_write(FMDD->ymfm, 0x105, 0);              /* turn OPL3 mode off again */
   }
   else {

      if (ctx->midi_card == MIDI_2XOPL2) {     /* if we have a second OPL2... */
	 for (i=0x1F5; i>0x100; i--)
	    fm_write(FMDD->ymfm, i, 0);

	 fm_write(FMDD->ymfm, 0x101, 0x20); 
	 fm_write(FMDD->ymfm, 0x1BD, 0xC0); 
      }
   }

   for (i=0; i<midi_driver->voices; i++) {
      FMDD->fm_key[i] = 0;
      FMDD->fm_keyscale[i] = 0;
      FMDD->fm_feedback[i] = 0;
      FMDD->fm_level[i] = 0;
      FMDD->fm_patch[i] = -1;
      fm_write(FMDD->ymfm, 0x40+fm_offset[i], 63);
      fm_write(FMDD->ymfm, 0x43+fm_offset[i], 63);
   }

   for (i=0; i<5; i++) {
      FMDD->fm_drum_cached_inst1[i] = NULL;
      FMDD->fm_drum_cached_inst2[i] = NULL;
      FMDD->fm_drum_cached_vol1[i] = -1;
      FMDD->fm_drum_cached_vol2[i] = -1;
      FMDD->fm_drum_cached_time[i] = 0;
   }

   fm_write(FMDD->ymfm, 0x01, 0x20);                  /* turn on wave form control */

   FMDD->fm_drum_mode = FALSE;
   FMDD->fm_drum_mask = 0xC0;
   fm_write(FMDD->ymfm, 0xBD, FMDD->fm_drum_mask);          /* set AM and vibrato to high */

   ctx->xmin = -1;
   ctx->xmax = -1;
}

END_OF_STATIC_FUNCTION(fm_reset);



/* fm_set_drum_mode:
 *  Switches the OPL synth between normal and percussion modes.
 */
static void fm_set_drum_mode(struct KDR_MIDI_CTX *ctx, int usedrums)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   int i;

   FMDD->fm_drum_mode = usedrums;
   FMDD->fm_drum_mask = usedrums ? 0xE0 : 0xC0;

   ctx->xmin = usedrums ? 6 : -1;
   ctx->xmax = usedrums ? 8 : -1;

   for (i=6; i<9; i++)
      if (ctx->midi_card == MIDI_OPL3)
	 fm_write(FMDD->ymfm, 0xC0+VOICE_OFFSET(i), 0x30);
      else
	 fm_write(FMDD->ymfm, 0xC0+VOICE_OFFSET(i), 0);

   fm_write(FMDD->ymfm, 0xBD, FMDD->fm_drum_mask);
}

END_OF_STATIC_FUNCTION(fm_set_drum_mode);



/* fm_set_voice:
 *  Sets the sound to be used for the specified voice, from a structure
 *  containing eleven bytes of FM operator data. Note that it doesn't
 *  actually set the volume: it just stores volume data in the fm_level
 *  arrays for fm_set_volume() to use.
 */
static INLINE void fm_set_voice(KDR_MIDI_CTX *ctx, int voice, FM_INSTRUMENT *inst)
{
   /* store some info */
   FMDD->fm_keyscale[voice] = inst->level2 & 0xC0;
   FMDD->fm_level[voice] = 63 - (inst->level2 & 63);
   FMDD->fm_feedback[voice] = inst->feedback;

   /* write the new data */
   fm_write(FMDD->ymfm, 0x20+fm_offset[voice], inst->characteristic1);
   fm_write(FMDD->ymfm, 0x23+fm_offset[voice], inst->characteristic2);
   fm_write(FMDD->ymfm, 0x60+fm_offset[voice], inst->attackdecay1);
   fm_write(FMDD->ymfm, 0x63+fm_offset[voice], inst->attackdecay2);
   fm_write(FMDD->ymfm, 0x80+fm_offset[voice], inst->sustainrelease1);
   fm_write(FMDD->ymfm, 0x83+fm_offset[voice], inst->sustainrelease2);
   fm_write(FMDD->ymfm, 0xE0+fm_offset[voice], inst->wave1);
   fm_write(FMDD->ymfm, 0xE3+fm_offset[voice], inst->wave2);

   /* don't set operator1 level for additive synthesis sounds */
   if (!(inst->feedback & 1))
      fm_write(FMDD->ymfm, 0x40+fm_offset[voice], inst->level1);

   /* on OPL3, 0xC0 contains pan info, so don't set it until fm_key_on() */
   if (ctx->midi_card != MIDI_OPL3)
      fm_write(FMDD->ymfm, 0xC0+VOICE_OFFSET(voice), inst->feedback);
}



/* fm_set_drum_op1:
 *  Sets the sound for operator #1 of a drum channel.
 */
static INLINE void fm_set_drum_op1(KDR_MIDI_CTX *ctx, int voice, FM_INSTRUMENT *inst)
{
   fm_write(FMDD->ymfm, 0x20+fm_offset[voice], inst->characteristic1);
   fm_write(FMDD->ymfm, 0x60+fm_offset[voice], inst->attackdecay1);
   fm_write(FMDD->ymfm, 0x80+fm_offset[voice], inst->sustainrelease1);
   fm_write(FMDD->ymfm, 0xE0+fm_offset[voice], inst->wave1);
}



/* fm_set_drum_op2:
 *  Sets the sound for operator #2 of a drum channel.
 */
static INLINE void fm_set_drum_op2(KDR_MIDI_CTX *ctx, int voice, FM_INSTRUMENT *inst)
{
   fm_write(FMDD->ymfm, 0x23+fm_offset[voice], inst->characteristic2);
   fm_write(FMDD->ymfm, 0x63+fm_offset[voice], inst->attackdecay2);
   fm_write(FMDD->ymfm, 0x83+fm_offset[voice], inst->sustainrelease2);
   fm_write(FMDD->ymfm, 0xE3+fm_offset[voice], inst->wave2);
}



/* fm_set_drum_vol_op1:
 *  Sets the volume for operator #1 of a drum channel.
 */
static INLINE void fm_set_drum_vol_op1(KDR_MIDI_CTX *ctx, int voice, int vol)
{
   vol = 63 * fm_vol_table[vol] / 128;
   fm_write(FMDD->ymfm, 0x40+fm_offset[voice], (63-vol));
}



/* fm_set_drum_vol_op2:
 *  Sets the volume for operator #2 of a drum channel.
 */
static INLINE void fm_set_drum_vol_op2(KDR_MIDI_CTX *ctx, int voice, int vol)
{
   vol = 63 * fm_vol_table[vol] / 128;
   fm_write(FMDD->ymfm, 0x43+fm_offset[voice], (63-vol));
}



/* fm_set_drum_pitch:
 *  Sets the pitch of a drum channel.
 */
static INLINE void fm_set_drum_pitch(KDR_MIDI_CTX *ctx, int voice, FM_INSTRUMENT *drum)
{
   fm_write(FMDD->ymfm, 0xA0+VOICE_OFFSET(voice), drum->freq);
   fm_write(FMDD->ymfm, 0xB0+VOICE_OFFSET(voice), drum->key & 0x1F);
}



/* fm_trigger_drum:
 *  Triggers a note on a drum channel.
 */
static INLINE void fm_trigger_drum(KDR_MIDI_CTX *ctx, int inst, int vol)
{
   FM_INSTRUMENT *drum = FMDD->fm_drum+inst;
   int d;

   if (!FMDD->fm_drum_mode)
      fm_set_drum_mode(ctx, TRUE);

   if (drum->type == FM_BD)
      d = 0;
   else if (drum->type == FM_SD)
      d = 1;
   else if (drum->type == FM_TT)
      d = 2;
   else if (drum->type == FM_CY)
      d = 3;
   else
      d = 4;

   /* don't let drum sounds come too close together */
   if (FMDD->fm_drum_cached_time[d] == ctx->_midi_tick)
      return;

   FMDD->fm_drum_cached_time[d] = ctx->_midi_tick;

   FMDD->fm_drum_mask &= (~drum->type);
   fm_write(FMDD->ymfm, 0xBD, FMDD->fm_drum_mask);

   vol = vol*3/4;

   if (fm_drum_op1[d]) {
      if (FMDD->fm_drum_cached_inst1[d] != drum) {
	 FMDD->fm_drum_cached_inst1[d] = drum;
	 fm_set_drum_op1(ctx, fm_drum_channel[d], drum);
      }

      if (FMDD->fm_drum_cached_vol1[d] != vol) {
	 FMDD->fm_drum_cached_vol1[d] = vol;
	 fm_set_drum_vol_op1(ctx, fm_drum_channel[d], vol);
      }
   }

   if (fm_drum_op2[d]) {
      if (FMDD->fm_drum_cached_inst2[d] != drum) {
	 FMDD->fm_drum_cached_inst2[d] = drum;
	 fm_set_drum_op2(ctx, fm_drum_channel[d], drum);
      }

      if (FMDD->fm_drum_cached_vol2[d] != vol) {
	 FMDD->fm_drum_cached_vol2[d] = vol;
	 fm_set_drum_vol_op2(ctx, fm_drum_channel[d], vol);
      }
   }

   fm_set_drum_pitch(ctx, fm_drum_channel[d], drum);

   FMDD->fm_drum_mask |= drum->type;
   fm_write(FMDD->ymfm, 0xBD, FMDD->fm_drum_mask);
}



/* fm_key_on:
 *  Triggers the specified voice. The instrument is specified as a GM
 *  patch number, pitch as a midi note number, and volume from 0-127.
 *  The bend parameter is _not_ expressed as a midi pitch bend value.
 *  It ranges from 0 (no pitch change) to 0xFFF (almost a semitone sharp).
 *  Drum sounds are indicated by passing an instrument number greater than
 *  128, in which case the sound is GM percussion key #(inst-128).
 */
static void fm_key_on(KDR_MIDI_CTX *ctx, int inst, int note, int bend, int vol, int pan)
{
   int voice;
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;

   if (inst > 127) {                               /* drum sound? */
      inst -= 163;
      if (inst < 0)
	 inst = 0;
      else if (inst > 46)
	 inst = 46;

      fm_trigger_drum(ctx, inst, vol);
   }
   else {                                          /* regular instrument */
      if (ctx->midi_card == MIDI_2XOPL2) {
	 /* the SB Pro-1 has fixed pan positions per voice... */
	 if (pan < 64)
	    voice = _midi_allocate_voice(ctx, 0, 5);
	 else
	    voice = _midi_allocate_voice(ctx, 9, midi_driver->voices-1);
      }
      else
	 /* on other cards we can use any voices */
	 voice = _midi_allocate_voice(ctx, -1, -1);

      if (voice < 0)
	 return;

      /* make sure the voice isn't sounding */
      fm_write(FMDD->ymfm, 0x43+fm_offset[voice], 63);
      if (FMDD->fm_feedback[voice] & 1)
	 fm_write(FMDD->ymfm, 0x40+fm_offset[voice], 63);

      /* make sure the voice is set up with the right sound */
      if (inst != FMDD->fm_patch[voice]) {
	 fm_set_voice(ctx, voice, FMDD->fm_instrument+inst);
	 FMDD->fm_patch[voice] = inst;
      }

      /* set pan position */
      if (ctx->midi_card == MIDI_OPL3) {
	 if (pan < 48)
	    pan = 0x10;
	 else if (pan >= 80)
	    pan = 0x20;
	 else
	    pan = 0x30;

	 fm_write(FMDD->ymfm, 0xC0+VOICE_OFFSET(voice), pan | FMDD->fm_feedback[voice]);
      }

      /* and play the note */
      fm_set_pitch(ctx, voice, note, bend);
      fm_set_volume(ctx, voice, vol);
   }
}

END_OF_STATIC_FUNCTION(fm_key_on);



/* fm_key_off:
 *  Hey, guess what this does :-)
 */
static void fm_key_off(KDR_MIDI_CTX *ctx, int voice)
{
   fm_write(FMDD->ymfm, 0xB0+VOICE_OFFSET(voice), FMDD->fm_key[voice] & 0xDF);
}

END_OF_STATIC_FUNCTION(fm_key_off);



/* fm_set_volume:
 *  Sets the volume of the specified voice (vol range 0-127).
 */
static void fm_set_volume(KDR_MIDI_CTX *ctx, int voice, int vol)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   if(voice < 0 || voice >= midi_driver->voices) return;
   vol = FMDD->fm_level[voice] * fm_vol_table[vol] / 128;
   fm_write(FMDD->ymfm, 0x43+fm_offset[voice], (63-vol) | FMDD->fm_keyscale[voice]);
   if (FMDD->fm_feedback[voice] & 1)
      fm_write(FMDD->ymfm, 0x40+fm_offset[voice], (63-vol) | FMDD->fm_keyscale[voice]);
}

END_OF_STATIC_FUNCTION(fm_set_volume);



/* fm_set_pitch:
 *  Sets the pitch of the specified voice.
 */
static void fm_set_pitch(KDR_MIDI_CTX *ctx, int voice, int note, int bend)
{
   int oct = 1;
   int freq;

   note -= 24;
   while (note >= 12) {
      note -= 12;
      oct++;
   }

   freq = fm_freq[note];
   if (bend)
      freq += (fm_freq[note+1] - fm_freq[note]) * bend / 0x1000;

   FMDD->fm_key[voice] = (oct<<2) | (freq >> 8);

   fm_write(FMDD->ymfm, 0xA0+VOICE_OFFSET(voice), freq & 0xFF); 
   fm_write(FMDD->ymfm, 0xB0+VOICE_OFFSET(voice), FMDD->fm_key[voice] | 0x20);
}

END_OF_STATIC_FUNCTION(fm_set_pitch);



/* fm_load_patches:
 *  Called before starting to play a MIDI file, to check if we need to be
 *  in rhythm mode or not.
 */
static int fm_load_patches(KDR_MIDI_CTX *ctx, AL_CONST char *patches, AL_CONST char *drums)
{
   int i;
   int usedrums = FALSE;

   for (i=6; i<9; i++) {
      FMDD->fm_key[i] = 0;
      FMDD->fm_keyscale[i] = 0;
      FMDD->fm_feedback[i] = 0;
      FMDD->fm_level[i] = 0;
      FMDD->fm_patch[i] = -1;
      fm_write(FMDD->ymfm, 0x40+fm_offset[i], 63);
      fm_write(FMDD->ymfm, 0x43+fm_offset[i], 63);
   }

   for (i=0; i<5; i++) {
      FMDD->fm_drum_cached_inst1[i] = NULL;
      FMDD->fm_drum_cached_inst2[i] = NULL;
      FMDD->fm_drum_cached_vol1[i] = -1;
      FMDD->fm_drum_cached_vol2[i] = -1;
      FMDD->fm_drum_cached_time[i] = 0;
   }

   for (i=0; i<128; i++) {
      if (drums[i]) {
	 usedrums = TRUE;
	 break;
      }
   }

   fm_set_drum_mode(ctx, usedrums);

   return 0;
}

END_OF_STATIC_FUNCTION(fm_load_patches);



/* fm_set_mixer_volume:
 *  For SB-Pro cards, sets the mixer volume for FM output.
 */
//SHOULDN'T NORMALLY BE CALLED
static int fm_set_mixer_volume(KDR_MIDI_CTX *ctx, int volume)
{
   //return _sb_set_mixer(-1, volume);
   return 0;
}



/* fm_is_there:
 *  Checks for the presence of an OPL synth at the current port.
 */
//SHOULDN'T NORMALLY BE CALLED
static int fm_is_there(void)
{
   return TRUE;
}



/* fm_detect:
 *  Adlib detection routine.
 */
//SHOULDN'T NORMALLY BE CALLED
static int fm_detect(KDR_MIDI_CTX *ctx, int input)
{
   return TRUE;
}

#include <stdio.h>
#include <stdlib.h>
#include "kdr_packfile.h"
/* load_ibk:
 *  Reads in a .IBK patch set file, for use by the Adlib driver.
 */
int kdr_load_ibk(KDR_MIDI_CTX *ctx, AL_CONST char *filename, int drums)
{
   char sig[4];
   FM_INSTRUMENT *inst;
   int c, note, oct, skip, count;

   PACKFILE *f = pack_fopen(filename, F_READ);
   if (!f)
      return -1;

   pack_fread(sig, 4, f);
   if (memcmp(sig, "IBK\x1A", 4) != 0) {
      pack_fclose(f);
      return -1;
   }

   if (drums) {
      inst = FMDD->fm_drum;
      skip = 35;
      count = 47;
   }
   else {
      inst = FMDD->fm_instrument;
      skip = 0;
      count = 128;
   }

   for (c=0; c<skip*16; c++)
      pack_getc(f);

   for (c=0; c<count; c++) {
      inst->characteristic1 = pack_getc(f);
      inst->characteristic2 = pack_getc(f);
      inst->level1 = pack_getc(f);
      inst->level2 = pack_getc(f);
      inst->attackdecay1 = pack_getc(f);
      inst->attackdecay2 = pack_getc(f);
      inst->sustainrelease1 = pack_getc(f);
      inst->sustainrelease2 = pack_getc(f);
      inst->wave1 = pack_getc(f);
      inst->wave2 = pack_getc(f);
      inst->feedback = pack_getc(f);

      if (drums) {
	 switch (pack_getc(f)) {
	    case 6:  inst->type = FM_BD;  break;
	    case 7:  inst->type = FM_HH;  break;
	    case 8:  inst->type = FM_TT;  break;
	    case 9:  inst->type = FM_SD;  break;
	    case 10: inst->type = FM_CY;  break;
	    default: inst->type = 0;      break;
	 }

	 pack_getc(f);

	 note = pack_getc(f) - 24;
	 oct = 1;

	 while (note >= 12) {
	    note -= 12;
	    oct++;
	 }

	 inst->freq = fm_freq[note];
	 inst->key = (oct<<2) | (fm_freq[note] >> 8);
      }
      else {
	 inst->type = 0;
	 inst->freq = 0;
	 inst->key = 0;

	 pack_getc(f);
	 pack_getc(f);
	 pack_getc(f);
      }

      pack_getc(f);
      pack_getc(f);

      inst++;
   }

   pack_fclose(f);
   return 0;
}


/* fm_init:
 *  Setup the adlib driver.
 */
static int fm_init(KDR_MIDI_CTX *ctx, int input, int voices, void *param)
{
   char tmp1[128], tmp2[128];
   AL_CONST char *s;
   int i;
   
   ctx->driver_data = malloc(sizeof(fm_driver_data));
   memset(ctx->driver_data, 0, sizeof(fm_driver_data));
   
   FMDD->ymfm = param;

   fm_reset(ctx, 1);
   
   memcpy(FMDD->fm_drum, fm_drum_preset, sizeof(fm_drum_preset));
   memcpy(FMDD->fm_instrument, fm_instrument_preset, sizeof(fm_instrument_preset));
   
   return 0;
}



/* fm_exit:
 *  Cleanup when we are finished.
 */
static void fm_exit(KDR_MIDI_CTX *ctx, int input)
{
   fm_reset(ctx, 0);
   
   free(ctx->driver_data);
}
