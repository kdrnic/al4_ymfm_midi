#ifndef KDR_MIDI_H
#define KDR_MIDI_H

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
 *      Some definitions for internal use by the library code.
 *
 *      By Shawn Hargreaves.
 *
 *      See readme.txt for copyright information.
 */


#ifdef __cplusplus
   extern "C" {
#endif

#define KDR_AL_CONST     const
#define KDR__AL_DLL  // __declspec(dllimport)
#define KDR_AL_FUNC(type, name, args)      KDR__AL_DLL type __cdecl name args
#define KDR_AL_VAR(type, name)             extern KDR__AL_DLL type name
#define KDR_AL_ID(a,b,c,d)     (((a)<<24) | ((b)<<16) | ((c)<<8) | (d))
#define KDR_AL_METHOD(type, name, args)    type (__cdecl *name) args
#define KDR_AL_FUNCPTR(type, name, args)            extern type (*name) args

#define KDR_AL_PROP_FUNCPTR(type, name, args)            type (*name) args
#define KDR_AL_PROP_VAR(type, name)              type name

/* maximum number of layers in a single voice */
#define KDR_MIDI_LAYERS  4
										/* Theoretical maximums: */
#define KDR_MIDI_VOICES           64    /* actual drivers may not be */
#define KDR_MIDI_TRACKS           32    /* able to handle this many */

typedef struct KDR_MIDI                    /* a midi file */
{
   int divisions;                      /* number of ticks per quarter note */
   struct {
      unsigned char *data;             /* MIDI message stream */
      int len;                         /* length of the track data */
   } track[KDR_MIDI_TRACKS];
} KDR_MIDI;

struct KDR_MIDI_CTX;

typedef struct KDR_MIDI_DRIVER             /* driver for playing midi music */
{
	int  id;                            /* driver ID code */
	KDR_AL_CONST char *name;            /* driver name */
	KDR_AL_CONST char *desc;            /* description string */
	KDR_AL_CONST char *ascii_name;      /* ASCII format name string */
	int  voices;                        /* available voices */
	int  basevoice;                     /* voice number offset */
	int  max_voices;                    /* maximum voices we can support */
	int  def_voices;                    /* default number of voices to use */
	
	/* setup routines */
	KDR_AL_METHOD(int,  detect, (struct KDR_MIDI_CTX *ctx, int input));
	KDR_AL_METHOD(int,  init, (struct KDR_MIDI_CTX *ctx, int input, int voices));
	KDR_AL_METHOD(void, exit, (struct KDR_MIDI_CTX *ctx, int input));
	KDR_AL_METHOD(int,  set_mixer_volume, (struct KDR_MIDI_CTX *ctx, int volume));
	KDR_AL_METHOD(int,  get_mixer_volume, (struct KDR_MIDI_CTX *ctx));
	
	/* raw MIDI output to MPU-401, etc. */
	KDR_AL_METHOD(void, raw_midi, (struct KDR_MIDI_CTX *ctx, int data));
	
	/* dynamic patch loading routines */
	KDR_AL_METHOD(int,  load_patches, (struct KDR_MIDI_CTX *ctx, KDR_AL_CONST char *patches, KDR_AL_CONST char *drums));
	KDR_AL_METHOD(void, adjust_patches, (struct KDR_MIDI_CTX *ctx, KDR_AL_CONST char *patches, KDR_AL_CONST char *drums));
	
	/* note control functions */
	KDR_AL_METHOD(void, key_on, (struct KDR_MIDI_CTX *ctx, int inst, int note, int bend, int vol, int pan));
	KDR_AL_METHOD(void, key_off, (struct KDR_MIDI_CTX *ctx, int voice));
	KDR_AL_METHOD(void, set_volume, (struct KDR_MIDI_CTX *ctx, int voice, int vol));
	KDR_AL_METHOD(void, set_pitch, (struct KDR_MIDI_CTX *ctx, int voice, int note, int bend));
	KDR_AL_METHOD(void, set_pan, (struct KDR_MIDI_CTX *ctx, int voice, int pan));
	KDR_AL_METHOD(void, set_vibrato, (struct KDR_MIDI_CTX *ctx, int voice, int amount));
} KDR_MIDI_DRIVER;

typedef long long int KDR_LARGE_INT;
_Static_assert(sizeof(KDR_LARGE_INT) >= 8, "KDR_LARGE_INT too small");

typedef struct KDR_MIDI_TRACK                   /* a track in the MIDI file */
{
   unsigned char *pos;                          /* position in track data */
   long timer;                                  /* time until next event */
   unsigned char running_status;                /* last MIDI event */
} KDR_MIDI_TRACK;


typedef struct KDR_MIDI_CHANNEL                 /* a MIDI channel */
{
   int patch;                                   /* current sound */
   int volume;                                  /* volume controller */
   int pan;                                     /* pan position */
   int pitch_bend;                              /* pitch bend position */
   int new_volume;                              /* cached volume change */
   int new_pitch_bend;                          /* cached pitch bend */
   int note[128][KDR_MIDI_LAYERS];              /* status of each note */
} KDR_MIDI_CHANNEL;

typedef struct KDR_MIDI_VOICE                   /* a voice on the soundcard */
{
   int channel;                                 /* MIDI channel */
   int note;                                    /* note (-1 = off) */
   int volume;                                  /* note velocity */
   long time;                                   /* when note was triggered */
} KDR_MIDI_VOICE;


typedef struct KDR_WAITING_NOTE                 /* a stored note-on request */
{
   int channel;
   int note;
   int volume;
} KDR_WAITING_NOTE;


typedef struct KDR_PATCH_TABLE                  /* GM -> external synth */
{
   int bank1;                                   /* controller #0 */
   int bank2;                                   /* controller #32 */
   int prog;                                    /* program change */
   int pitch;                                   /* pitch shift */
} KDR_PATCH_TABLE;

typedef struct KDR_MIDI_CTX
{
	KDR_LARGE_INT
		int_time,
		int_speed,
		int_last;
	
	const KDR_MIDI_DRIVER *midi_driver;
	int midi_card;
	long _midi_tick;
	
	KDR_AL_PROP_VAR(volatile long, midi_pos);       /* current position in the midi file, in beats */
	KDR_AL_PROP_VAR(volatile long, midi_time);      /* current position in the midi file, in seconds */
	KDR_AL_PROP_VAR(long,          midi_loop_start);         /* where to loop back to at EOF */
	KDR_AL_PROP_VAR(long,          midi_loop_end);           /* loop when we hit this position */
	
	KDR_AL_PROP_FUNCPTR(void,      midi_msg_callback, (struct KDR_MIDI_CTX *ctx, int msg, int byte1, int byte2));
	KDR_AL_PROP_FUNCPTR(void,      midi_meta_callback, (struct KDR_MIDI_CTX *ctx, int type, KDR_AL_CONST unsigned char *data, int length));
	KDR_AL_PROP_FUNCPTR(void,      midi_sysex_callback, (struct KDR_MIDI_CTX *ctx, KDR_AL_CONST unsigned char *data, int length));
	
	//Taken from KDR_MIDI_DRIVER to force it used as const
	int xmin, xmax;
	
	//Former static globalss from midi.c ----------------------------------------------------------------------
	KDR_MIDI *midifile;                      /* the file that is playing */
	
	int _midi_volume;
	
	long midi_timers;                        /* current position in allegro-timer-ticks */
	long midi_pos_counter;                   /* delta for midi_pos */
	int midi_loop;                           /* repeat at eof? */
	
	int midi_semaphore;                      /* reentrancy flag */
	int midi_loaded_patches;                 /* loaded entire patch set? */
	
	long midi_timer_speed;                   /* midi_player's timer speed */
	int midi_pos_speed;                      /* MIDI delta -> midi_pos */
	int midi_speed;                          /* MIDI delta -> timer */
	int midi_new_speed;                      /* for tempo change events */
	
	int old_midi_volume;                     /* stored global volume */
	
	int midi_alloc_channel;                  /* so _midi_allocate_voice */
	int midi_alloc_note;                     /* knows which note the */
	int midi_alloc_vol;                      /* sound is associated with */
	
	struct KDR_MIDI_TRACK midi_track[KDR_MIDI_TRACKS];     /* the active tracks */
	struct KDR_MIDI_VOICE midi_voice[KDR_MIDI_VOICES];     /* synth voice status */
	struct KDR_MIDI_CHANNEL midi_channel[16];              /* MIDI channel info */
	struct KDR_WAITING_NOTE midi_waiting[KDR_MIDI_VOICES]; /* notes still to be played */
	struct KDR_PATCH_TABLE patch_table[128];               /* GM -> external synth */
	
	int midi_seeking;                        /* set during seeks */
	int midi_looping;                        /* set during loops */
} KDR_MIDI_CTX;

#define KDR_MIDI_OPL2             KDR_AL_ID('O','P','L','2')
#define KDR_MIDI_2XOPL2           KDR_AL_ID('O','P','L','X')
#define KDR_MIDI_OPL3             KDR_AL_ID('O','P','L','3')

extern const KDR_MIDI_DRIVER kdr_midi_opl3, kdr_midi_opl2, kdr_midi_2xopl2;

KDR_MIDI_CTX *kdr_create_midi_ctx(void);
void kdr_destroy_midi_ctx(KDR_MIDI_CTX *ctx);
void kdr_install_driver(KDR_MIDI_CTX *ctx, const KDR_MIDI_DRIVER *drv);
int kdr_load_ibk(KDR_MIDI_CTX *ctx, const char *filename, int drums);

KDR_AL_FUNC(KDR_MIDI *,   kdr_load_midi,        (KDR_MIDI_CTX *ctx, KDR_AL_CONST char *filename));
KDR_AL_FUNC(void,         kdr_destroy_midi,     (KDR_MIDI_CTX *ctx, KDR_MIDI *midi));
KDR_AL_FUNC(int,          kdr_play_midi,        (KDR_MIDI_CTX *ctx, KDR_MIDI *midi, int loop));
KDR_AL_FUNC(int,          kdr_play_looped_midi, (KDR_MIDI_CTX *ctx, KDR_MIDI *midi, int loop_start, int loop_end));
KDR_AL_FUNC(void,         kdr_stop_midi,        (KDR_MIDI_CTX *ctx));
KDR_AL_FUNC(void,         kdr_midi_pause,       (KDR_MIDI_CTX *ctx));
KDR_AL_FUNC(void,         kdr_midi_resume,      (KDR_MIDI_CTX *ctx));
KDR_AL_FUNC(int,          kdr_midi_seek,        (KDR_MIDI_CTX *ctx, int target));
KDR_AL_FUNC(int,          kdr_get_midi_length,  (KDR_MIDI_CTX *ctx, KDR_MIDI *midi));
KDR_AL_FUNC(void,         kdr_midi_out,         (KDR_MIDI_CTX *ctx, unsigned char *data, int length));
KDR_AL_FUNC(int,          kdr_load_midi_patches,(KDR_MIDI_CTX *ctx));
KDR_AL_FUNC(void,         kdr_update_midi,      (KDR_MIDI_CTX *ctx, int samples, int sampl_rate));

#ifdef KDR_INTERNAL
	// MACROS NEEDED ---------------------------------------------------------------
	#if 1
	#define AL_CONST       KDR_AL_CONST 
	#define _AL_DLL        KDR__AL_DLL  
	#define AL_FUNC        KDR_AL_FUNC  
	#define AL_VAR         KDR_AL_VAR   
	#define AL_ID          KDR_AL_ID    
	#define AL_METHOD      KDR_AL_METHOD
	#define AL_FUNCPTR     KDR_AL_FUNCPTR
	
	#define EMPTY_STRING    "\0\0\0"
	
	#ifndef TRUE
	#define TRUE 1
	#endif
	#ifndef FALSE
	#define FALSE 0
	#endif
	
	#include <assert.h>
	#ifndef ASSERT
	#define ASSERT assert
	#endif
	
	/* fill in default memory locking macros */
	#ifndef END_OF_FUNCTION
	#define END_OF_FUNCTION(x)
	#define END_OF_STATIC_FUNCTION(x)
	#define LOCK_DATA(d, s)
	#define LOCK_CODE(c, s)
	#define UNLOCK_DATA(d, s)
	#define LOCK_VARIABLE(x)
	#define LOCK_FUNCTION(x)
	#endif
	
	#define INLINE
	
	#define ABS(x)       (((x) >= 0) ? (x) : (-(x)))
	
	#define F_READ          "r"
	#define TIMERS_PER_SECOND     1193181L
	#define BPS_TO_TIMER(x)       (TIMERS_PER_SECOND / (long)(x))
	#define _AL_MALLOC(SIZE)         (malloc(SIZE))
	#define _AL_MALLOC_ATOMIC(SIZE)  (malloc(SIZE))
	#define _AL_FREE(PTR)            (free(PTR))
	#define _AL_REALLOC(PTR, SIZE)   (realloc(PTR, SIZE))
	
	#endif
	
	#define MIDI        KDR_MIDI
	#define MIDI_TRACKS KDR_MIDI_TRACKS
	#define MIDI_VOICES KDR_MIDI_VOICES
	#define MIDI_DRIVER KDR_MIDI_DRIVER
	#define MIDI_LAYERS KDR_MIDI_LAYERS
	
	// STUFF FROM Allegro's midi.h -------------------------------------------------
	#if 1
	
	#define MIDI_AUTODETECT       -1
	#define MIDI_NONE             0
	#define MIDI_DIGMID           AL_ID('D','I','G','I')
	
	#define load_midi               kdr_load_midi
	#define destroy_midi            kdr_destroy_midi
	#define play_midi               kdr_play_midi
	#define play_looped_midi        kdr_play_looped_midi
	#define stop_midi               kdr_stop_midi
	#define midi_pause              kdr_midi_pause
	#define midi_resume             kdr_midi_resume
	#define midi_seek               kdr_midi_seek
	#define get_midi_length         kdr_get_midi_length
	#define midi_out                kdr_midi_out
	#define load_midi_patches       kdr_load_midi_patches
	#define _midi_allocate_voice    kdr_midi_allocate_voice
	#define _dummy_noop2            kdr_dummy_noop2
	#define _dummy_adjust_patches   kdr_dummy_adjust_patches
	#define _dummy_detect           kdr_dummy_detect
	#define _dummy_init             kdr_dummy_init
	#define _dummy_exit             kdr_dummy_exit
	#define _dummy_set_mixer_volume kdr_dummy_set_mixer_volume
	#define _dummy_get_mixer_volume kdr_dummy_get_mixer_volume
	#define _dummy_raw_midi         kdr_dummy_raw_midi
	#define _dummy_load_patches     kdr_dummy_load_patches
	#define _dummy_key_on           kdr_dummy_key_on
	#define _dummy_noop1            kdr_dummy_noop1
	#define _dummy_noop3            kdr_dummy_noop3
	#define midi_player             kdr_midi_player
	
	AL_FUNC(int, _midi_allocate_voice, (KDR_MIDI_CTX *ctx, int min, int max));
	
	int  _dummy_detect(struct KDR_MIDI_CTX *ctx, int input) ;
	int  _dummy_init(struct KDR_MIDI_CTX *ctx, int input, int voices) ;
	void _dummy_exit(struct KDR_MIDI_CTX *ctx, int input) ;
	int  _dummy_set_mixer_volume(struct KDR_MIDI_CTX *ctx, int volume) ;
	int  _dummy_get_mixer_volume(struct KDR_MIDI_CTX *ctx) ;
	void _dummy_noop1(struct KDR_MIDI_CTX *ctx, int p) ;
	void _dummy_noop2(struct KDR_MIDI_CTX *ctx, int p1, int p2) ;
	void _dummy_noop3(struct KDR_MIDI_CTX *ctx, int p1, int p2, int p3) ;
	void _dummy_raw_midi(struct KDR_MIDI_CTX *ctx, int data) ;
	int  _dummy_load_patches(struct KDR_MIDI_CTX *ctx, AL_CONST char *patches, AL_CONST char *drums) ;
	void _dummy_adjust_patches(struct KDR_MIDI_CTX *ctx, AL_CONST char *patches, AL_CONST char *drums) ;
	void _dummy_key_on(struct KDR_MIDI_CTX *ctx, int inst, int note, int bend, int vol, int pan) ;

	#endif
#endif

#ifdef __cplusplus
   }
#endif

#endif          /* ifndef AINTERN_H */
