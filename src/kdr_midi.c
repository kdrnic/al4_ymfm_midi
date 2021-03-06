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
 *      The core MIDI file player.
 *
 *      By Shawn Hargreaves.
 *
 *      Pause and seek functions by George Foot.
 *
 *      get_midi_length by Elias Pschernig.
 *
 *      See readme.txt for copyright information.
 */


#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//#include "allegro.h"
#define KDR_INTERNAL
#include "kdr_midi.h"

int  _dummy_detect(struct KDR_MIDI_CTX *ctx, int input) { return TRUE; }
int  _dummy_init(struct KDR_MIDI_CTX *ctx, int input, int voices, void *param) { /*digi_none.desc = _midi_none.desc = get_config_text("The sound of silence");*/ return 0; }
void _dummy_exit(struct KDR_MIDI_CTX *ctx, int input) { }
int  _dummy_set_mixer_volume(struct KDR_MIDI_CTX *ctx, int volume) { return 0; }
int  _dummy_get_mixer_volume(struct KDR_MIDI_CTX *ctx) { return -1; }
void _dummy_noop1(struct KDR_MIDI_CTX *ctx, int p) { }
void _dummy_noop2(struct KDR_MIDI_CTX *ctx, int p1, int p2) { }
void _dummy_noop3(struct KDR_MIDI_CTX *ctx, int p1, int p2, int p3) { }
void _dummy_raw_midi(struct KDR_MIDI_CTX *ctx, int data) { }
int  _dummy_load_patches(struct KDR_MIDI_CTX *ctx, AL_CONST char *patches, AL_CONST char *drums) { return 0; }
void _dummy_adjust_patches(struct KDR_MIDI_CTX *ctx, AL_CONST char *patches, AL_CONST char *drums) { }
void _dummy_key_on(struct KDR_MIDI_CTX *ctx, int inst, int note, int bend, int vol, int pan) { }

static MIDI_DRIVER _midi_none =
{
   MIDI_NONE,
   EMPTY_STRING,
   EMPTY_STRING,
   "No sound", 
   0, 0, 0xFFFF, 0,
   _dummy_detect,
   _dummy_init,
   _dummy_exit,
   _dummy_set_mixer_volume,
   _dummy_get_mixer_volume,
   _dummy_raw_midi,
   _dummy_load_patches,
   _dummy_adjust_patches,
   _dummy_key_on,
   _dummy_noop1,
   _dummy_noop2,
   _dummy_noop3,
   _dummy_noop2,
   _dummy_noop2
};

/* how often the midi callback gets called maximally / second */
#define MIDI_TIMER_FREQUENCY 40

static void midi_player(KDR_MIDI_CTX *ctx);                  /* core MIDI player routine */
static void prepare_to_play(KDR_MIDI_CTX *ctx, MIDI *midi);

/*
KDRNIC's solution:
	Simulate PIT interruption based timers through a function called with the number of new samples added
*/
#define MSEC_TO_TIMER(x)      ((long)(x) * (TIMERS_PER_SECOND / 1000))
static void install_int(KDR_MIDI_CTX *ctx, void *proc, long speed)
{
	if(proc == midi_player){
		ctx->int_speed = MSEC_TO_TIMER(speed);
	}
}
static void install_int_ex(KDR_MIDI_CTX *ctx, void *proc, long speed)
{
	if(proc == midi_player){
		ctx->int_speed = speed;
	}
}
static void remove_int(KDR_MIDI_CTX *ctx, void *proc)
{
	if(proc == midi_player){
		ctx->int_speed = 0;
	}
}
void kdr_update_midi(KDR_MIDI_CTX *ctx, int samples, int sampl_rate)
{
	ctx->int_time += ((KDR_LARGE_INT) samples * TIMERS_PER_SECOND) / (KDR_LARGE_INT) sampl_rate;
	if(ctx->int_speed && ctx->int_time - ctx->int_last > ctx->int_speed){
		midi_player(ctx);
		ctx->int_last = ctx->int_time;
	}
}

#include "kdr_packfile.h"

/* load_midi:
 *  Loads a standard MIDI file, returning a pointer to a MIDI structure,
 *  or NULL on error. 
 */
MIDI *load_midi(KDR_MIDI_CTX *ctx, AL_CONST char *filename)
{
   int c;
   char buf[4];
   long data;
   
   PACKFILE *fp;
   
   MIDI *midi;
   int num_tracks;
   ASSERT(filename);

   fp = pack_fopen(filename, F_READ);        /* open the file */
   if (!fp)
      return NULL;

   midi = _AL_MALLOC(sizeof(MIDI));              /* get some memory */
   memset(midi, 0, sizeof(MIDI));
   if (!midi) {
      pack_fclose(fp);
      return NULL;
   }

   for (c=0; c<MIDI_TRACKS; c++) {
      midi->track[c].data = NULL;
      midi->track[c].len = 0;
   }

   pack_fread(buf, 4, fp); /* read midi header */

   /* Is the midi inside a .rmi file? */
   if (memcmp(buf, "RIFF", 4) == 0) { /* check for RIFF header */
      pack_mgetl(fp);

      while (!pack_feof(fp)) {
         pack_fread(buf, 4, fp); /* RMID chunk? */
         if (memcmp(buf, "RMID", 4) == 0) break;

         pack_fseek(fp, pack_igetl(fp)); /* skip to next chunk */
      }

      if (pack_feof(fp)) goto err;

      pack_mgetl(fp);
      pack_mgetl(fp);
      pack_fread(buf, 4, fp); /* read midi header */
   }

   if (memcmp(buf, "MThd", 4))
      goto err;

   pack_mgetl(fp);                           /* skip header chunk length */

   data = pack_mgetw(fp);                    /* MIDI file type */
   if ((data != 0) && (data != 1))
      goto err;

   num_tracks = pack_mgetw(fp);              /* number of tracks */
   if ((num_tracks < 1) || (num_tracks > MIDI_TRACKS))
      goto err;

   data = pack_mgetw(fp);                    /* beat divisions */
   midi->divisions = ABS(data);

   for (c=0; c<num_tracks; c++) {            /* read each track */
      pack_fread(buf, 4, fp);                /* read track header */
      if (memcmp(buf, "MTrk", 4))
	 goto err;

      data = pack_mgetl(fp);                 /* length of track chunk */
      midi->track[c].len = data;

      midi->track[c].data = _AL_MALLOC_ATOMIC(data); /* allocate memory */
	  memset(midi->track[c].data, 0, data);
      if (!midi->track[c].data)
	 goto err;
					     /* finally, read track data */
      if (pack_fread(midi->track[c].data, data, fp) != data)
	 goto err;
   }

   pack_fclose(fp);
   return midi;

   /* oh dear... */
   err:
   pack_fclose(fp);
   destroy_midi(ctx, midi);
   return NULL;
}



/* destroy_midi:
 *  Frees the memory being used by a MIDI file.
 */
void destroy_midi(KDR_MIDI_CTX *ctx, MIDI *midi)
{
   int c;

   if (midi == ctx->midifile)
      stop_midi(ctx);

   if (midi) {
      for (c=0; c<MIDI_TRACKS; c++) {
	 if (midi->track[c].data) {
	    UNLOCK_DATA(midi->track[c].data, midi->track[c].len);
	    _AL_FREE(midi->track[c].data);
	 }
      }

      UNLOCK_DATA(midi, sizeof(MIDI));
      _AL_FREE(midi);
   }
}



/* parse_var_len:
 *  The MIDI file format is a strange thing. Time offsets are only 32 bits,
 *  yet they are compressed in a weird variable length format. This routine 
 *  reads a variable length integer from a MIDI data stream. It returns the 
 *  number read, and alters the data pointer according to the number of
 *  bytes it used.
 */
static unsigned long parse_var_len(AL_CONST unsigned char **data)
{
   unsigned long val = **data & 0x7F;

   while (**data & 0x80) {
      (*data)++;
      val <<= 7;
      val += (**data & 0x7F);
   }

   (*data)++;
   return val;
}

END_OF_STATIC_FUNCTION(parse_var_len);



/* global_volume_fix:
 *  Converts a note volume, adjusting it according to the global 
 *  ctx->_midi_volume variable.
 */
static INLINE int global_volume_fix(KDR_MIDI_CTX *ctx, int vol)
{
   if (ctx->_midi_volume >= 0)
      return (vol * ctx->_midi_volume) / 256;

   return vol;
}



/* sort_out_volume:
 *  Converts a note volume, adjusting it according to the channel volume
 *  and the global ctx->_midi_volume variable.
 */
static INLINE int sort_out_volume(KDR_MIDI_CTX *ctx, int c, int vol)
{
   
   return global_volume_fix(ctx, (vol * ctx->midi_channel[c].volume) / 128);
}



/* raw_program_change:
 *  Sends a program change message to a device capable of handling raw
 *  MIDI data, using patch mapping tables. Assumes that midi_driver->raw_midi
 *  isn't NULL, so check before calling it!
 */
static void raw_program_change(KDR_MIDI_CTX *ctx, int channel, int patch)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   
   if (channel != 9) {
      /* bank change #1 */
      if (ctx->patch_table[patch].bank1 >= 0) {
	 midi_driver->raw_midi(ctx, 0xB0+channel);
	 midi_driver->raw_midi(ctx, 0);
	 midi_driver->raw_midi(ctx, ctx->patch_table[patch].bank1);
      }

      /* bank change #2 */
      if (ctx->patch_table[patch].bank2 >= 0) {
	 midi_driver->raw_midi(ctx, 0xB0+channel);
	 midi_driver->raw_midi(ctx, 32);
	 midi_driver->raw_midi(ctx, ctx->patch_table[patch].bank2);
      }

      /* program change */
      midi_driver->raw_midi(ctx, 0xC0+channel);
      midi_driver->raw_midi(ctx, ctx->patch_table[patch].prog);

      /* update volume */
      midi_driver->raw_midi(ctx, 0xB0+channel);
      midi_driver->raw_midi(ctx, 7);
      midi_driver->raw_midi(ctx, global_volume_fix(ctx, ctx->midi_channel[channel].volume-1));
   }
}

END_OF_STATIC_FUNCTION(raw_program_change);



/* midi_note_off:
 *  Processes a MIDI note-off event.
 */
static void midi_note_off(KDR_MIDI_CTX *ctx, int channel, int note)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   int done = FALSE;
   int voice, layer;
   int c;

   /* can we send raw MIDI data? */
   if (midi_driver->raw_midi) {
      if (channel != 9)
	 note += ctx->patch_table[ctx->midi_channel[channel].patch].pitch;

      midi_driver->raw_midi(ctx, 0x80+channel);
      midi_driver->raw_midi(ctx, note);
      midi_driver->raw_midi(ctx, 0);
      return;
   }

   /* oh well, have to do it the long way... */
   for (layer=0; layer<MIDI_LAYERS; layer++) {
      voice = ctx->midi_channel[channel].note[note][layer];
      if (voice >= 0) {
	 midi_driver->key_off(ctx, voice + midi_driver->basevoice);
	 ctx->midi_voice[voice].note = -1;
	 ctx->midi_voice[voice].time = ctx->_midi_tick;
	 ctx->midi_channel[channel].note[note][layer] = -1; 
	 done = TRUE;
      }
   }

   /* if the note isn't playing, it must still be in the waiting room */
   if (!done) {
      for (c=0; c<MIDI_VOICES; c++) {
	 if ((ctx->midi_waiting[c].channel == channel) && 
	     (ctx->midi_waiting[c].note == note)) {
	    ctx->midi_waiting[c].note = -1;
	    break;
	 }
      }
   }
}

END_OF_STATIC_FUNCTION(midi_note_off);



/* sort_out_pitch_bend:
 *  MIDI pitch bend range is + or - two semitones. The low-level soundcard
 *  drivers can only handle bends up to +1 semitone. This routine converts
 *  pitch bends from MIDI format to our own format.
 */
static INLINE void sort_out_pitch_bend(int *bend, int *note)
{
   if (*bend == 0x2000) {
      *bend = 0;
      return;
   }

   (*bend) -= 0x2000;

   while (*bend < 0) {
      (*note)--;
      (*bend) += 0x1000;
   }

   while (*bend >= 0x1000) {
      (*note)++;
      (*bend) -= 0x1000;
   }
}



/* _midi_allocate_voice:
 *  Allocates a MIDI voice in the range min-max (inclusive). This is 
 *  intended to be called by the key_on() handlers in the MIDI driver, 
 *  and shouldn't be used by any other code.
 */
int _midi_allocate_voice(KDR_MIDI_CTX *ctx, int min, int max)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   int c;
   int layer;
   int voice = -1;
   long best_time = LONG_MAX;

   if (min < 0)
      min = 0;

   if (max < 0)
      max = midi_driver->voices-1;

   /* which layer can we use? */
   for (layer=0; layer<MIDI_LAYERS; layer++)
      if (ctx->midi_channel[ctx->midi_alloc_channel].note[ctx->midi_alloc_note][layer] < 0)
	 break; 

   if (layer >= MIDI_LAYERS)
      return -1;

   /* find a free voice */
   for (c=min; c<=max; c++) {
      if ((ctx->midi_voice[c].note < 0) && 
	  (ctx->midi_voice[c].time < best_time) && 
	  ((c < ctx->xmin) || (c > ctx->xmax))) {
	 voice = c;
	 best_time = ctx->midi_voice[c].time;
      }
   }

   /* if there are no free voices, kill a note to make room */
   if (voice < 0) {
      voice = -1;
      best_time = LONG_MAX;
      for (c=min; c<=max; c++) {
	 if ((ctx->midi_voice[c].time < best_time) && 
	     ((c < ctx->xmin) || (c > ctx->xmax))) {
	    voice = c;
	    best_time = ctx->midi_voice[c].time;
	 }
      }
      if (voice >= 0)
	 midi_note_off(ctx, ctx->midi_voice[voice].channel, ctx->midi_voice[voice].note);
      else
	 return -1;
   }

   /* ok, we got it... */
   ctx->midi_voice[voice].channel = ctx->midi_alloc_channel;
   ctx->midi_voice[voice].note = ctx->midi_alloc_note;
   ctx->midi_voice[voice].volume = ctx->midi_alloc_vol;
   ctx->midi_voice[voice].time = ctx->_midi_tick;
   ctx->midi_channel[ctx->midi_alloc_channel].note[ctx->midi_alloc_note][layer] = voice; 

   return voice + midi_driver->basevoice;
}

END_OF_FUNCTION(_midi_allocate_voice);



/* midi_note_on:
 *  Processes a MIDI note-on event. Tries to find a free soundcard voice,
 *  and if it can't either cuts off an existing note, or if 'polite' is
 *  set, just stores the channel, note and volume in the waiting list.
 */
static void midi_note_on(KDR_MIDI_CTX *ctx, int channel, int note, int vol, int polite)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   int c, layer, inst, bend, corrected_note;

   /* it's easy if the driver can handle raw MIDI data */
   if (midi_driver->raw_midi) {
      if (channel != 9)
	 note += ctx->patch_table[ctx->midi_channel[channel].patch].pitch;

      midi_driver->raw_midi(ctx, 0x90+channel);
      midi_driver->raw_midi(ctx, note);
      midi_driver->raw_midi(ctx, vol);
      return;
   }

   /* if the note is already on, turn it off */
   for (layer=0; layer<MIDI_LAYERS; layer++) {
      if (ctx->midi_channel[channel].note[note][layer] >= 0) {
	 midi_note_off(ctx, channel, note);
	 return;
      }
   }

   /* if zero volume and the note isn't playing, we can just ignore it */
   if (vol == 0)
      return;

   if (channel != 9) {
      /* are there any free voices? */
      for (c=0; c<midi_driver->voices; c++)
	 if ((ctx->midi_voice[c].note < 0) && 
	     ((c < ctx->xmin) || (c > ctx->xmax)))
	    break;

      /* if there are no free voices, remember the note for later */
      if ((c >= midi_driver->voices) && (polite)) {
	 for (c=0; c<MIDI_VOICES; c++) {
	    if (ctx->midi_waiting[c].note < 0) {
	       ctx->midi_waiting[c].channel = channel;
	       ctx->midi_waiting[c].note = note;
	       ctx->midi_waiting[c].volume = vol;
	       break;
	    }
	 }
	 return;
      }
   }

   /* drum sound? */
   if (channel == 9) {
      inst = 128+note;
      corrected_note = 60;
      bend = 0;
   }
   else {
      inst = ctx->midi_channel[channel].patch;
      corrected_note = note;
      bend = ctx->midi_channel[channel].pitch_bend;
      sort_out_pitch_bend(&bend, &corrected_note);
   }

   /* play the note */
   ctx->midi_alloc_channel = channel;
   ctx->midi_alloc_note = note;
   ctx->midi_alloc_vol = vol;

   midi_driver->key_on(ctx, inst, corrected_note, bend, 
		       sort_out_volume(ctx, channel, vol), 
		       ctx->midi_channel[channel].pan);
}

END_OF_STATIC_FUNCTION(midi_note_on);



/* all_notes_off:
 *  Turns off all active notes.
 */
static void all_notes_off(KDR_MIDI_CTX *ctx, int channel)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   
   if (midi_driver->raw_midi) {
      midi_driver->raw_midi(ctx, 0xB0+channel);
      midi_driver->raw_midi(ctx, 123);
      midi_driver->raw_midi(ctx, 0);
      return;
   }
   else {
      int note, layer;

      for (note=0; note<128; note++)
	 for (layer=0; layer<MIDI_LAYERS; layer++)
	    if (ctx->midi_channel[channel].note[note][layer] >= 0)
	       midi_note_off(ctx, channel, note);
   }
}

END_OF_STATIC_FUNCTION(all_notes_off);



/* all_sound_off:
 *  Turns off sound.
 */
static void all_sound_off(KDR_MIDI_CTX *ctx, int channel)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   
   if (midi_driver->raw_midi) {
      midi_driver->raw_midi(ctx, 0xB0+channel);
      midi_driver->raw_midi(ctx, 120);
      midi_driver->raw_midi(ctx, 0);
      return;
   }
}

END_OF_STATIC_FUNCTION(all_sound_off);



/* reset_controllers:
 *  Resets volume, pan, pitch bend, etc, to default positions.
 */
static void reset_controllers(KDR_MIDI_CTX *ctx, int channel)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   
   ctx->midi_channel[channel].new_volume = 128;
   ctx->midi_channel[channel].new_pitch_bend = 0x2000;

   if (midi_driver->raw_midi) {
      midi_driver->raw_midi(ctx, 0xB0+channel);
      midi_driver->raw_midi(ctx, 121);
      midi_driver->raw_midi(ctx, 0);
   }

   switch (channel % 3) {
      case 0:  ctx->midi_channel[channel].pan = ((channel/3) & 1) ? 60 : 68; break;
      case 1:  ctx->midi_channel[channel].pan = 104; break;
      case 2:  ctx->midi_channel[channel].pan = 24; break;
   }

   if (midi_driver->raw_midi) {
      midi_driver->raw_midi(ctx, 0xB0+channel);
      midi_driver->raw_midi(ctx, 10);
      midi_driver->raw_midi(ctx, ctx->midi_channel[channel].pan);
   }
}

END_OF_STATIC_FUNCTION(reset_controllers);



/* update_controllers:
 *  Checks cached controller information and updates active voices.
 */
static void update_controllers(KDR_MIDI_CTX *ctx)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   int c, c2, vol, bend, note;

   for (c=0; c<16; c++) {
      /* check for volume controller change */
      if ((ctx->midi_channel[c].volume != ctx->midi_channel[c].new_volume) || (ctx->old_midi_volume != ctx->_midi_volume)) {
	 ctx->midi_channel[c].volume = ctx->midi_channel[c].new_volume;
	 if (midi_driver->raw_midi) {
	    midi_driver->raw_midi(ctx, 0xB0+c);
	    midi_driver->raw_midi(ctx, 7);
	    midi_driver->raw_midi(ctx, global_volume_fix(ctx, ctx->midi_channel[c].volume-1));
	 }
	 else {
	    for (c2=0; c2<MIDI_VOICES; c2++) {
	       if ((ctx->midi_voice[c2].channel == c) && (ctx->midi_voice[c2].note >= 0)) {
		  vol = sort_out_volume(ctx, c, ctx->midi_voice[c2].volume);
		  midi_driver->set_volume(ctx, c2 + midi_driver->basevoice, vol);
	       }
	    }
	 }
      }

      /* check for pitch bend change */
      if (ctx->midi_channel[c].pitch_bend != ctx->midi_channel[c].new_pitch_bend) {
	 ctx->midi_channel[c].pitch_bend = ctx->midi_channel[c].new_pitch_bend;
	 if (midi_driver->raw_midi) {
	    midi_driver->raw_midi(ctx, 0xE0+c);
	    midi_driver->raw_midi(ctx, ctx->midi_channel[c].pitch_bend & 0x7F);
	    midi_driver->raw_midi(ctx, ctx->midi_channel[c].pitch_bend >> 7);
	 }
	 else {
	    for (c2=0; c2<MIDI_VOICES; c2++) {
	       if ((ctx->midi_voice[c2].channel == c) && (ctx->midi_voice[c2].note >= 0)) {
		  bend = ctx->midi_channel[c].pitch_bend;
		  note = ctx->midi_voice[c2].note;
		  sort_out_pitch_bend(&bend, &note);
		  midi_driver->set_pitch(ctx, c2 + midi_driver->basevoice, note, bend);
	       }
	    }
	 }
      }
   }

   ctx->old_midi_volume = ctx->_midi_volume;
}

END_OF_STATIC_FUNCTION(update_controllers);



/* process_controller:
 *  Deals with a MIDI controller message on the specified channel.
 */
static void process_controller(KDR_MIDI_CTX *ctx, int channel, int ctrl, int data)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   
   switch (ctrl) {

      case 7:                                   /* main volume */
	 ctx->midi_channel[channel].new_volume = data+1;
	 break;

      case 10:                                  /* pan */
	 ctx->midi_channel[channel].pan = data;
	 if (midi_driver->raw_midi) {
	    midi_driver->raw_midi(ctx, 0xB0+channel);
	    midi_driver->raw_midi(ctx, 10);
	    midi_driver->raw_midi(ctx, data);
	 }
	 break;

      case 120:                                 /* all sound off */
	 all_sound_off(ctx, channel);
	 break;

      case 121:                                 /* reset all controllers */
	 reset_controllers(ctx, channel);
	 break;

      case 123:                                 /* all notes off */
      case 124:                                 /* omni mode off */
      case 125:                                 /* omni mode on */
      case 126:                                 /* poly mode off */
      case 127:                                 /* poly mode on */
	 all_notes_off(ctx, channel);
	 break;

      default:
	 if (midi_driver->raw_midi) {
	    midi_driver->raw_midi(ctx, 0xB0+channel);
	    midi_driver->raw_midi(ctx, ctrl);
	    midi_driver->raw_midi(ctx, data);
	 }
	 break;
   }
}

END_OF_STATIC_FUNCTION(process_controller);



/* process_meta_event:
 *  Processes the next meta-event on the specified track.
 */
static void process_meta_event(KDR_MIDI_CTX *ctx, AL_CONST unsigned char **pos, long *timer)
{
   unsigned char metatype = *((*pos)++);
   long length = parse_var_len(pos);
   long tempo;

   if (ctx->midi_meta_callback)
      ctx->midi_meta_callback(ctx, metatype, *pos, length);

   if (metatype == 0x2F) {                      /* end of track */
      *pos = NULL;
      *timer = LONG_MAX;
      return;
   }

   if (metatype == 0x51) {                      /* tempo change */
      tempo = (*pos)[0] * 0x10000L + (*pos)[1] * 0x100 + (*pos)[2];
      ctx->midi_new_speed = (tempo/1000) * (TIMERS_PER_SECOND/1000);
      ctx->midi_new_speed /= ctx->midifile->divisions;
   }

   (*pos) += length;
}

END_OF_STATIC_FUNCTION(process_meta_event);



/* process_midi_event:
 *  Processes the next MIDI event on the specified track.
 */
static void process_midi_event(KDR_MIDI_CTX *ctx, AL_CONST unsigned char **pos, unsigned char *running_status, long *timer)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   unsigned char byte1, byte2; 
   int channel;
   unsigned char event;
   long l;

   event = *((*pos)++); 

   if (event & 0x80) {                          /* regular message */
      /* no running status for sysex and meta-events! */
      if ((event != 0xF0) && (event != 0xF7) && (event != 0xFF))
	 *running_status = event;
      byte1 = (*pos)[0];
      byte2 = (*pos)[1];
   }
   else {                                       /* use running status */
      byte1 = event; 
      byte2 = (*pos)[0];
      event = *running_status; 
      (*pos)--;
   }

   /* program callback? */
   if ((ctx->midi_msg_callback) && 
       (event != 0xF0) && (event != 0xF7) && (event != 0xFF))
      ctx->midi_msg_callback(ctx, event, byte1, byte2);

   channel = event & 0x0F;

   switch (event>>4) {

      case 0x08:                                /* note off */
	 midi_note_off(ctx, channel, byte1);
	 (*pos) += 2;
	 break;

      case 0x09:                                /* note on */
	 midi_note_on(ctx, channel, byte1, byte2, 1);
	 (*pos) += 2;
	 break;

      case 0x0A:                                /* note aftertouch */
	 (*pos) += 2;
	 break;

      case 0x0B:                                /* control change */
	 process_controller(ctx, channel, byte1, byte2);
	 (*pos) += 2;
	 break;

      case 0x0C:                                /* program change */
	 ctx->midi_channel[channel].patch = byte1;
	 if (midi_driver->raw_midi)
	    raw_program_change(ctx, channel, byte1);
	 (*pos) += 1;
	 break;

      case 0x0D:                                /* channel aftertouch */
	 (*pos) += 1;
	 break;

      case 0x0E:                                /* pitch bend */
	 ctx->midi_channel[channel].new_pitch_bend = byte1 + (byte2<<7);
	 (*pos) += 2;
	 break;

      case 0x0F:                                /* special event */
	 switch (event) {
	    case 0xF0:                          /* sysex */
	    case 0xF7: 
	       l = parse_var_len(pos);
	       if (ctx->midi_sysex_callback)
		  ctx->midi_sysex_callback(ctx, *pos, l);
	       (*pos) += l;
	       break;

	    case 0xF2:                          /* song position */
	       (*pos) += 2;
	       break;

	    case 0xF3:                          /* song select */
	       (*pos)++;
	       break;

	    case 0xFF:                          /* meta-event */
	       process_meta_event(ctx, pos, timer);
	       break;

	    default:
	       /* the other special events don't have any data bytes,
		  so we don't need to bother skipping past them */
	       break;
	 }
	 break;

      default:
	 /* something has gone badly wrong if we ever get to here */
	 break;
   }
}

END_OF_STATIC_FUNCTION(process_midi_event);



/* midi_player:
 *  The core MIDI player: to be used as a timer callback.
 */
static void midi_player(KDR_MIDI_CTX *ctx)
{
   int c;
   long l;
   int active;

   if (!ctx->midifile)
      return;

  
   if (ctx->midi_semaphore) {
      ctx->midi_timer_speed += BPS_TO_TIMER(MIDI_TIMER_FREQUENCY);
      install_int_ex(ctx, midi_player, BPS_TO_TIMER(MIDI_TIMER_FREQUENCY));
      return;
   }
   

   ctx->midi_semaphore = TRUE;
   ctx->_midi_tick++;

   ctx->midi_timers += ctx->midi_timer_speed;
   ctx->midi_time = ctx->midi_timers / TIMERS_PER_SECOND;

   do_it_all_again:

   for (c=0; c<MIDI_VOICES; c++)
      ctx->midi_waiting[c].note = -1;

   /* deal with each track in turn... */
   for (c=0; c<MIDI_TRACKS; c++) { 
      if (ctx->midi_track[c].pos) {
	 ctx->midi_track[c].timer -= ctx->midi_timer_speed;

	 /* while events are waiting, process them */
	 while (ctx->midi_track[c].timer <= 0) { 
	    process_midi_event(ctx, (AL_CONST unsigned char**) &ctx->midi_track[c].pos, 
			       &ctx->midi_track[c].running_status,
			       &ctx->midi_track[c].timer); 

	    /* read next time offset */
	    if (ctx->midi_track[c].pos) { 
	       l = parse_var_len((AL_CONST unsigned char**) &ctx->midi_track[c].pos);
	       l *= ctx->midi_speed;
	       ctx->midi_track[c].timer += l;
	    }
	 }
      }
   }

   /* update global position value */
   ctx->midi_pos_counter -= ctx->midi_timer_speed;
   while (ctx->midi_pos_counter <= 0) {
      ctx->midi_pos_counter += ctx->midi_pos_speed;
      ctx->midi_pos++;
   }

   /* tempo change? */
   if (ctx->midi_new_speed > 0) {
      for (c=0; c<MIDI_TRACKS; c++) {
	 if (ctx->midi_track[c].pos) {
	    ctx->midi_track[c].timer /= ctx->midi_speed;
	    ctx->midi_track[c].timer *= ctx->midi_new_speed;
	 }
      }
      ctx->midi_pos_counter /= ctx->midi_speed;
      ctx->midi_pos_counter *= ctx->midi_new_speed;

      ctx->midi_speed = ctx->midi_new_speed;
      ctx->midi_pos_speed = ctx->midi_new_speed * ctx->midifile->divisions;
      ctx->midi_new_speed = -1;
   }

   /* figure out how long until we need to be called again */
   active = 0;
   ctx->midi_timer_speed = LONG_MAX;
   for (c=0; c<MIDI_TRACKS; c++) {
      if (ctx->midi_track[c].pos) {
	 active = 1;
	 if (ctx->midi_track[c].timer < ctx->midi_timer_speed)
	    ctx->midi_timer_speed = ctx->midi_track[c].timer;
      }
   }

   /* end of the music? */
   if ((!active) || ((ctx->midi_loop_end > 0) && (ctx->midi_pos >= ctx->midi_loop_end))) {
      if ((ctx->midi_loop) && (!ctx->midi_looping)) {
	 if (ctx->midi_loop_start > 0) {
	    remove_int(ctx, midi_player);
	    ctx->midi_semaphore = FALSE;
	    ctx->midi_looping = TRUE;
	    if (midi_seek(ctx, ctx->midi_loop_start) != 0) {
	       ctx->midi_looping = FALSE;
	       stop_midi(ctx); 
	       return;
	    }
	    ctx->midi_looping = FALSE;
	    ctx->midi_semaphore = TRUE;
	    goto do_it_all_again;
	 }
	 else {
	    for (c=0; c<16; c++) {
	       all_notes_off(ctx, c);
	       all_sound_off(ctx, c);
	    }
	    prepare_to_play(ctx, ctx->midifile);
	    goto do_it_all_again;
	 }
      }
      else {
	 stop_midi(ctx); 
	 ctx->midi_semaphore = FALSE;
	 return;
      }
   }

   /* reprogram the timer */
   if (ctx->midi_timer_speed < BPS_TO_TIMER(MIDI_TIMER_FREQUENCY))
      ctx->midi_timer_speed = BPS_TO_TIMER(MIDI_TIMER_FREQUENCY);

   if (!ctx->midi_seeking) 
      install_int_ex(ctx, midi_player, ctx->midi_timer_speed);

   /* controller changes are cached and only processed here, so we can 
      condense streams of controller data into just a few voice updates */ 
   update_controllers(ctx);

   /* and deal with any notes that are still waiting to be played */
   for (c=0; c<MIDI_VOICES; c++)
      if (ctx->midi_waiting[c].note >= 0)
	 midi_note_on(ctx, ctx->midi_waiting[c].channel, ctx->midi_waiting[c].note,
		      ctx->midi_waiting[c].volume, 0);

   ctx->midi_semaphore = FALSE;
}

END_OF_STATIC_FUNCTION(midi_player);



/* midi_init:
 *  Sets up the MIDI player ready for use. Returns non-zero on failure.
 */
static int midi_init(KDR_MIDI_CTX *ctx)
{
   int c, c2, c3;
   char **argv;
   int argc;
   char buf[32], tmp[64];

   ctx->midi_loaded_patches = FALSE;

   for (c=0; c<16; c++) {
      ctx->midi_channel[c].volume = ctx->midi_channel[c].new_volume = 128;
      ctx->midi_channel[c].pitch_bend = ctx->midi_channel[c].new_pitch_bend = 0x2000;

      for (c2=0; c2<128; c2++)
	 for (c3=0; c3<MIDI_LAYERS; c3++)
	    ctx->midi_channel[c].note[c2][c3] = -1;
   }

   for (c=0; c<MIDI_VOICES; c++) {
      ctx->midi_voice[c].note = -1;
      ctx->midi_voice[c].time = 0;
   }
    
   #if 0
   for (c=0; c<128; c++) {
      uszprintf(buf, sizeof(buf), uconvert_ascii("p%d", tmp), c+1);
      argv = get_config_argv(uconvert_ascii("midimap", tmp), buf, &argc);

      if ((argv) && (argc == 4)) {
	 ctx->patch_table[c].bank1 = ustrtol(argv[0], NULL, 0);
	 ctx->patch_table[c].bank2 = ustrtol(argv[1], NULL, 0);
	 ctx->patch_table[c].prog  = ustrtol(argv[2], NULL, 0);
	 ctx->patch_table[c].pitch = ustrtol(argv[3], NULL, 0);
      }
      else {
	 ctx->patch_table[c].bank1 = -1;
	 ctx->patch_table[c].bank2 = -1;
	 ctx->patch_table[c].prog = c;
	 ctx->patch_table[c].pitch = 0;
      }
   }
   #endif

   //register_datafile_object(DAT_MIDI, NULL, (void (*)(void *))destroy_midi);

   return 0;
}



/* load_patches:
 *  Scans through a MIDI file and identifies which patches it uses, passing
 *  them to the soundcard driver so it can load whatever samples are
 *  neccessary.
 */
static int load_patches(KDR_MIDI_CTX *ctx, MIDI *midi)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   
   char patches[128], drums[128];
   unsigned char *p, *end;
   unsigned char running_status, event;
   long l;
   int c;
   ASSERT(midi);

   for (c=0; c<128; c++)                        /* initialise to unused */
      patches[c] = drums[c] = FALSE;

   patches[0] = TRUE;                           /* always load the piano */

   for (c=0; c<MIDI_TRACKS; c++) {              /* for each track... */
      p = midi->track[c].data;
      end = p + midi->track[c].len;
      running_status = 0;

      while (p < end) {                         /* work through data stream */
#ifdef ALLEGRO_BEOS
         /* Is there a bug in this routine, or in gcc under BeOS/x86? --PW */
         { int i; for (i=1; i; i--); }
#endif
	 event = *p; 
	 if (event & 0x80) {                    /* regular message */
	    p++;
	    if ((event != 0xF0) && (event != 0xF7) && (event != 0xFF))
	       running_status = event;
	 }
	 else                                   /* use running status */
	    event = running_status; 

	 switch (event>>4) {

	    case 0x0C:                          /* program change! */
	       patches[*p] = TRUE;
	       p++;
	       break;

	    case 0x09:                          /* note on, is it a drum? */
	       if ((event & 0x0F) == 9)
		  drums[*p] = TRUE;
	       p += 2;
	       break;

	    case 0x08:                          /* note off */
	    case 0x0A:                          /* note aftertouch */
	    case 0x0B:                          /* control change */
	    case 0x0E:                          /* pitch bend */
	       p += 2;
	       break;

	    case 0x0D:                          /* channel aftertouch */
	       p += 1;
	       break;

	    case 0x0F:                          /* special event */
	       switch (event) {
		  case 0xF0:                    /* sysex */
		  case 0xF7: 
		     l = parse_var_len((AL_CONST unsigned char**) &p);
		     p += l;
		     break;

		  case 0xF2:                    /* song position */
		     p += 2;
		     break;

		  case 0xF3:                    /* song select */
		     p++;
		     break;

		  case 0xFF:                    /* meta-event */
		     p++;
		     l = parse_var_len((AL_CONST unsigned char**) &p);
		     p += l;
		     break;

		  default:
		     /* the other special events don't have any data bytes,
			so we don't need to bother skipping past them */
		     break;
	       }
	       break;

	    default:
	       /* something has gone badly wrong if we ever get to here */
	       break;
	 }

	 if (p < end)                           /* skip time offset */
	    parse_var_len((AL_CONST unsigned char**) &p);
      }
   }

   /* tell the driver to do its stuff */ 
   return midi_driver->load_patches(ctx, patches, drums);
}



/* prepare_to_play:
 *  Sets up all the global variables needed to play the specified file.
 */
static void prepare_to_play(KDR_MIDI_CTX *ctx, MIDI *midi)
{
   const KDR_MIDI_DRIVER *midi_driver = ctx->midi_driver;
   
   int c;
   ASSERT(midi);

   for (c=0; c<16; c++)
      reset_controllers(ctx, c);

   update_controllers(ctx);

   ctx->midifile = midi;
   ctx->midi_pos = 0;
   ctx->midi_timers = 0;
   ctx->midi_time = 0;
   ctx->midi_pos_counter = 0;
   ctx->midi_speed = TIMERS_PER_SECOND / 2 / ctx->midifile->divisions;   /* 120 bpm */
   ctx->midi_new_speed = -1;
   ctx->midi_pos_speed = ctx->midi_speed * ctx->midifile->divisions;
   ctx->midi_timer_speed = 0;
   ctx->midi_seeking = 0;
   ctx->midi_looping = 0;

   for (c=0; c<16; c++) {
      ctx->midi_channel[c].patch = 0;
      if (midi_driver->raw_midi)
	 raw_program_change(ctx, c, 0);
   }

   for (c=0; c<MIDI_TRACKS; c++) {
      if (midi->track[c].data) {
	 ctx->midi_track[c].pos = midi->track[c].data;
	 ctx->midi_track[c].timer = parse_var_len((AL_CONST unsigned char**) &ctx->midi_track[c].pos);
	 ctx->midi_track[c].timer *= ctx->midi_speed;
      }
      else {
	 ctx->midi_track[c].pos = NULL;
	 ctx->midi_track[c].timer = LONG_MAX;
      }
      ctx->midi_track[c].running_status = 0;
   }
}

END_OF_STATIC_FUNCTION(prepare_to_play);



/* play_midi:
 *  Starts playing the specified MIDI file. If loop is set, the MIDI file 
 *  will be repeated until replaced with something else, otherwise it will 
 *  stop at the end of the file. Passing a NULL MIDI file will stop whatever 
 *  music is currently playing: allegro.h defines the macro stop_midi() to 
 *  be play_midi(NULL, FALSE); Returns non-zero if an error occurs (this
 *  may happen if a patch-caching wavetable driver is unable to load the
 *  required samples).
 */
int play_midi(KDR_MIDI_CTX *ctx, MIDI *midi, int loop)
{
   int c;

   remove_int(ctx, midi_player);

   for (c=0; c<16; c++) {
      all_notes_off(ctx, c);
      all_sound_off(ctx, c);
   }

   if (midi) {
      if (!ctx->midi_loaded_patches)
	 if (load_patches(ctx, midi) != 0)
	    return -1;

      ctx->midi_loop = loop;
      ctx->midi_loop_start = -1;
      ctx->midi_loop_end = -1;

      prepare_to_play(ctx, midi);

      /* arbitrary speed, midi_player() will adjust it */
      install_int(ctx, midi_player, 20);
   }
   else {
      ctx->midifile = NULL;

      if (ctx->midi_pos > 0)
	 ctx->midi_pos = -ctx->midi_pos;
      else if (ctx->midi_pos == 0)
	 ctx->midi_pos = -1;
   }

   return 0;
}

END_OF_FUNCTION(play_midi);



/* play_looped_midi:
 *  Like play_midi(), but the file loops from the specified end position
 *  back to the specified start position (the end position can be -1 to 
 *  indicate the end of the file).
 */
int play_looped_midi(KDR_MIDI_CTX *ctx, MIDI *midi, int loop_start, int loop_end)
{
   if (play_midi(ctx, midi, TRUE) != 0)
      return -1;

   ctx->midi_loop_start = loop_start;
   ctx->midi_loop_end = loop_end;

   return 0;
}



/* stop_midi:
 *  Stops whatever MIDI file is currently playing.
 */
void stop_midi(KDR_MIDI_CTX *ctx)
{
   play_midi(ctx, NULL, FALSE);
}

END_OF_FUNCTION(stop_midi);



/* midi_pause:
 *  Pauses the currently playing MIDI file.
 */
void midi_pause(KDR_MIDI_CTX *ctx)
{
   int c;

   if (!ctx->midifile)
      return;

   remove_int(ctx, midi_player);

   for (c=0; c<16; c++) {
      all_notes_off(ctx, c);
      all_sound_off(ctx, c);
   }
}

END_OF_FUNCTION(midi_pause);



/* midi_resume:
 *  Resumes playing a paused MIDI file.
 */
void midi_resume(KDR_MIDI_CTX *ctx)
{
   if (!ctx->midifile)
      return;

   install_int_ex(ctx, midi_player, ctx->midi_timer_speed);
}

END_OF_FUNCTION(midi_resume);



/* midi_seek:
 *  Seeks to the given midi_pos in the current MIDI file. If the target 
 *  is earlier in the file than the current midi_pos it seeks from the 
 *  beginning; otherwise it seeks from the current position. Returns zero 
 *  if successful, non-zero if it hit the end of the file (1 means it 
 *  stopped playing, 2 means it looped back to the start).
 */
int midi_seek(KDR_MIDI_CTX *ctx, int target)
{
   int old_midi_loop;
   MIDI *old_midifile;
   const MIDI_DRIVER *old_driver;
   int old_patch[16];
   int old_volume[16];
   int old_pan[16];
   int old_pitch_bend[16];
   int c;

   if (!ctx->midifile)
      return -1;

   /* first stop the player */
   midi_pause(ctx);

   /* store current settings */
   for (c=0; c<16; c++) {
      old_patch[c] = ctx->midi_channel[c].patch;
      old_volume[c] = ctx->midi_channel[c].volume;
      old_pan[c] = ctx->midi_channel[c].pan;
      old_pitch_bend[c] = ctx->midi_channel[c].pitch_bend;
   }

   /* save some variables and give temporary values */
   old_driver = ctx->midi_driver;
   ctx->midi_driver = &_midi_none;
   old_midi_loop = ctx->midi_loop;
   ctx->midi_loop = 0;
   old_midifile = ctx->midifile;

   /* set flag to tell midi_player not to reinstall itself */
   ctx->midi_seeking = 1;

   /* are we seeking backwards? If so, skip back to the start of the file */
   if (target <= ctx->midi_pos)
      prepare_to_play(ctx, ctx->midifile);

   /* now sit back and let midi_player get to the position */
   while ((ctx->midi_pos < target) && (ctx->midi_pos >= 0)) {
      int mmpc = ctx->midi_pos_counter;
      int mmp = ctx->midi_pos;

      mmpc -= ctx->midi_timer_speed;
      while (mmpc <= 0) {
	 mmpc += ctx->midi_pos_speed;
	 mmp++;
      }

      if (mmp >= target)
	 break;

      midi_player(ctx);
   }

   /* restore previously saved variables */
   ctx->midi_loop = old_midi_loop;
   ctx->midi_driver = old_driver;
   ctx->midi_seeking = 0;

   if (ctx->midi_pos >= 0) {
      /* refresh the driver with any changed parameters */
      if (ctx->midi_driver->raw_midi) {
	 for (c=0; c<16; c++) {
	    /* program change (this sets the volume as well) */
	    if ((ctx->midi_channel[c].patch != old_patch[c]) ||
		(ctx->midi_channel[c].volume != old_volume[c]))
	       raw_program_change(ctx, c, ctx->midi_channel[c].patch);

	    /* pan */
	    if (ctx->midi_channel[c].pan != old_pan[c]) {
	       ctx->midi_driver->raw_midi(ctx, 0xB0+c);
	       ctx->midi_driver->raw_midi(ctx, 10);
	       ctx->midi_driver->raw_midi(ctx, ctx->midi_channel[c].pan);
	    }

	    /* pitch bend */
	    if (ctx->midi_channel[c].pitch_bend != old_pitch_bend[c]) {
	       ctx->midi_driver->raw_midi(ctx, 0xE0+c);
	       ctx->midi_driver->raw_midi(ctx, ctx->midi_channel[c].pitch_bend & 0x7F);
	       ctx->midi_driver->raw_midi(ctx, ctx->midi_channel[c].pitch_bend >> 7);
	    }
	 }
      }

      /* if we didn't hit the end of the file, continue playing */
      if (!ctx->midi_looping)
	 install_int(ctx, midi_player, 20);

      return 0;
   }

   if ((ctx->midi_loop) && (!ctx->midi_looping)) {  /* was file looped? */
      prepare_to_play(ctx, old_midifile);
      install_int(ctx, midi_player, 20);
      return 2;                           /* seek past EOF => file restarted */
   }

   return 1;                              /* seek past EOF => file stopped */
}

END_OF_FUNCTION(midi_seek);



/* get_midi_length:
 *  Returns the length, in seconds, of the specified midi. This will stop any
 *  currently playing midi. Don't call it too often, since it simulates playing
 *  all of the midi to get the time even if the midi contains tempo changes.
 */
int get_midi_length(KDR_MIDI_CTX *ctx, MIDI *midi)
{
    play_midi(ctx, midi, 0);
    while (ctx->midi_pos < 0); /* Without this, midi_seek won't work. */
    midi_seek(ctx, INT_MAX);
    return ctx->midi_time;
}



/* midi_out:
 *  Inserts MIDI command bytes into the output stream, in realtime.
 */
void midi_out(KDR_MIDI_CTX *ctx, unsigned char *data, int length)
{
   unsigned char *pos = data;
   unsigned char running_status = 0;
   long timer = 0;
   ASSERT(data);

   ctx->midi_semaphore = TRUE;
   ctx->_midi_tick++;

   while (pos < data+length)
      process_midi_event(ctx, (AL_CONST unsigned char**) &pos, &running_status, &timer);

   update_controllers(ctx);

   ctx->midi_semaphore = FALSE;
}



/* load_midi_patches:
 *  Tells the MIDI driver to preload the entire sample set.
 */
int load_midi_patches(KDR_MIDI_CTX *ctx)
{
   char patches[128], drums[128];
   int c, ret;

   for (c=0; c<128; c++)
      patches[c] = drums[c] = TRUE;

   ctx->midi_semaphore = TRUE;
   ret = ctx->midi_driver->load_patches(ctx, patches, drums);
   ctx->midi_semaphore = FALSE;

   ctx->midi_loaded_patches = TRUE;

   return ret;
}

KDR_MIDI_CTX *kdr_create_midi_ctx(void)
{
	KDR_MIDI_CTX *ctx = malloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));
	
	ctx->_midi_volume = -1;
	ctx->old_midi_volume = -1;
	
	midi_init(ctx);
	
	return ctx;
}

void kdr_destroy_midi_ctx(KDR_MIDI_CTX *ctx)
{
	free(ctx);
}

void kdr_install_driver(KDR_MIDI_CTX *ctx, const KDR_MIDI_DRIVER *drv, void *param)
{
	ctx->xmin = -1;
	ctx->xmax = -1;
	ctx->midi_driver = drv;
	ctx->midi_card = drv->id;
	ctx->midi_driver->init(ctx, 0, ctx->midi_driver->voices, param);
}
