#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <allegro.h>

#include "ymfm_lib.h"
#include "kdr_adlib.h"
#include "wav.h"

int main(int argc, char **argv)
{
	//Some player state variables
	int i;
	char midi_fn[256] = "";
	MIDI *mid = 0;
	int nbeats, nsecs;
	int paused = 0;
	int seekam = 1;
	int firstseek = 0;
	int redrawbar = 0;
	int oldsp = 0;
	int close = 0;
	int maxsecs = INT_MAX;
	int wavonly = 0;
	
	//Handle command line arguments
	for(i = 1; i < argc; i++){
		char *arg = argv[i];
		if(arg[0] == '-'){
			#define STREQ(a, b) (strcmp((a), (b)) == 0)
			#define STRNEQLIT(a, b) (strncmp((a), (b), sizeof(b) - 1) == 0)
			#define ARGVAL (strchr(arg, '=') + 1)
			#define ARG1(a) (STREQ(arg, a))
			#define ARG2(a) (STRNEQLIT(arg, a))
			#define ARG(a) if(((a)[sizeof(a) - 2] == '=') ? ARG2(a) : ARG1(a))
			ARG("-seek=") firstseek = atoi(ARGVAL);
			ARG("-close") close = 1;
			ARG("-maxsecs="){
				close = 1;
				maxsecs = atoi(ARGVAL);
			}
			ARG("-wavonly") wavonly = 1;
		}
		else{
			strcpy(midi_fn, arg);
		}
	}
	
	//Allegro 4 initialization
	allegro_init();
	set_color_depth(32);
	set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 400, 0, 0);
	install_keyboard();
	install_mouse();
	install_timer();
	install_sound(DIGI_AUTODETECT, MIDI_NONE, 0);
	set_display_switch_mode(SWITCH_BACKGROUND);
	
	//Initialize OPL driver
	install_opl_midi(MIDI_OPL3);
	
	//Initialize audio stream
	int stereo = 1;
	int sampling_rate = 44100;
	uint16_t *audio_buf = 0;
	int audio_buf_siz = 1024;
	AUDIOSTREAM *audio_stream = 0;
	if(!wavonly) audio_stream = play_audio_stream(
		audio_buf_siz, //in samples not bytes, and ignoring stereo
		16,            //can be 8 or 16 bits
		stereo,        //if stereo, samples are interleaved, left,right,left,right
		sampling_rate, //in Hz
		255,           //volume
		128            //panning (0 left to 255 right)
	);
	else audio_buf = malloc(audio_buf_siz * sizeof(*audio_buf) * (stereo + 1));
	
	//Initialize YMFM
	ymfm_init(SB16_OPL_CLOCK_RATE, sampling_rate, stereo);
	
	//File selection dialog if file not passed as argument
	if(!strlen(midi_fn)){
		filesel:
		if(!file_select_ex("Select MIDI to play, Mr. MWF", midi_fn, "MID;/+a", sizeof(midi_fn), 0, 0)){
			if(alert("No MIDI file picked.", "Exit  now?", "AND LEAVE THE MUSIC IN GRAVE PERIL??", "Yes, I'll wimp out", "No, I am brave hero", 'y', 'n') == 1){
				goto end;
			}
			else goto filesel;
		}
	}
	//Load MIDI file
	mid = load_midi(midi_fn);
	if(!mid){
		if(alert("Couldn't load MIDI file.", "Maybe it is broken.", "Let's try again.", "Yes, please", "I am an idiot", 'y', 'n')){
			goto filesel;
		}
	}
	
	clear(screen);
	
	//Calculate MIDI duration
	get_midi_length(mid);
	nbeats = -midi_pos;
	nsecs = midi_time;
	
	//Initialize some stuff for WAV file capture
	int wavbuf_siz = (nsecs + 1) * sampling_rate * (1 + stereo), wavlen = 0;
	uint16_t *wavbuf = malloc(sizeof(*wavbuf) * wavbuf_siz);
	
	//Start playing
	play_midi(mid, 1);
	
	if(firstseek){
		midi_seek(firstseek);
	}
	
	rectfill(screen, 0, 100, SCREEN_W, 150, makecol(0, 0, 255));
	
	//Main loop
	while(!key[KEY_ESC] && !(close && (midi_time >= nsecs || midi_time >= maxsecs))){
		//START OF PLAYER UI CODE -----------------------------------------
		#if 1
		int cbeats, csecs, bar;
		int seekto, seek = 0;
		int multi = 1;
		cbeats = midi_pos;
		csecs = midi_time;
		
		if(key_shifts & KB_SHIFT_FLAG) multi = 4;
		
		textprintf_ex(screen, font, 0, 0, makecol(255, 255, 255), makecol(0, 0, 0), "MIDI length is %04d beats %04d seconds now at %04d beats %04d seconds.", nbeats, nsecs, cbeats, csecs);
		textprintf_ex(screen, font, 0, 8, makecol(255, 255, 255), makecol(0, 0, 0), "Press ESC to quit, Left-Right to seek, Shift increases seek speed, space pauses.");
		if(paused) textprintf_ex(screen, font, 0, 16, makecol(255, 255, 255), makecol(0, 0, 0), "PAUSED.");
		else textprintf_ex(screen, font, 0, 16, makecol(255, 255, 255), makecol(0, 0, 0), "PLAYING");
		
		bar = (SCREEN_W * csecs) / nsecs;
		if(redrawbar){
			rectfill(screen, 0, 100, SCREEN_W, 150, makecol(0, 0, 255));
			redrawbar = 0;
		}
		rectfill(screen, 0, 100, bar, 150, makecol(255, 32, 32));
		
		if(!paused){
			if(key[KEY_LEFT]){
				seek = 1;
				seekto = cbeats - seekam * multi;
				if(seekto < 0) seekto = 0;
			}
			if(key[KEY_RIGHT]){
				seek = 1;
				seekto = cbeats + seekam * multi;
				if(seekto > nbeats) seekto = nbeats;
			}
			if(seek){
				redrawbar = 1;
				midi_seek(seekto);
			}
			if(key[KEY_SPACE] && !oldsp){
				paused = 1;
				midi_pause();
			}
		}
		else{
			if(key[KEY_SPACE] && !oldsp){
				paused = 0;
				midi_resume();
			}
		}
		oldsp = key[KEY_SPACE];
		#endif
		//END OF PLAYER UI CODE -------------------------------------------
		
		//Use get_audio_stream_buffer to try and get a new buffer to fill with
		//n=audio_buf_siz samples
		if(wavonly || (audio_buf = (uint16_t *) get_audio_stream_buffer(audio_stream))){
			ymfm_generate(audio_buf, audio_buf_siz);
			
			//Also add to WAV file capture:
			int add_siz = audio_buf_siz * (1 + stereo);
			while(wavbuf_siz < wavlen + add_siz){
				//Grow wavbuf
				wavbuf_siz *= 3;
				wavbuf_siz /= 2;
				wavbuf = realloc(wavbuf, sizeof(*wavbuf) * wavbuf_siz);
			}
			memcpy(wavbuf + wavlen, audio_buf, add_siz * sizeof(*wavbuf));
			wavlen += add_siz;
			
			//Release buffer
			if(!wavonly) free_audio_stream_buffer(audio_stream);
		}
	}
	
	//Save WAV file
	char wav_fn[256];
	strcpy(wav_fn, get_filename(midi_fn));
	*(get_extension(wav_fn) - 1) = 0;
	strcat(wav_fn, YMFMLIB_RESAMPLE ? "_rsmpl" : "");
	strcat(wav_fn, YMFMLIB_TIME_REG_WRITES ? "_time" : "");
	strcat(wav_fn, ".wav");
	SAMPLE *wav_smpl = create_sample(16, stereo, sampling_rate, wavlen / (1 + stereo));
	memcpy(wav_smpl->data, wavbuf, wavlen * sizeof(*wavbuf));
	save_wav(wav_fn, wav_smpl);
	destroy_sample(wav_smpl);
	free(wavbuf);
	
	end:
	allegro_exit();
	printf("Bye, MWF. Come back soon.\n");
	return 0;
}
END_OF_MAIN()
