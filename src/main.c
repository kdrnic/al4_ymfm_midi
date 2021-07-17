#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <allegro.h>

#include "ymfm_lib.h"
#include "kdr_midi.h"
#include "wav.h"
#include "crc32.h"

int main(int argc, char **argv)
{
	//Some player state variables
	int i;
	char midi_fn[256] = "";
	KDR_MIDI *mid = 0;
	int nbeats, nsecs;
	int paused = 0;
	int seekam = 1;
	int firstseek = 0;
	int redrawbar = 0;
	int oldsp = 0;
	int close = 0;
	int maxsecs = INT_MAX;
	int wavonly = 0;
	char *ibk_fn = 0;
	char *ibkd_fn = 0;
	
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
			ARG("-ibk=") ibk_fn = ARGVAL;
			ARG("-ibkd=") ibkd_fn = ARGVAL;
		}
		else{
			strcpy(midi_fn, arg);
		}
	}
	
	//Allegro 4 initialization
	allegro_init();
	set_color_depth(32);
	set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 400, 0, 0);
	install_timer();
	install_keyboard();
	install_mouse();
	install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, 0);
	set_display_switch_mode(SWITCH_BACKGROUND);
	
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
	
	//Initialize OPL driver
	KDR_MIDI_CTX *ctx = kdr_create_midi_ctx();
	kdr_install_driver(ctx, &kdr_midi_opl3);
	if(ibk_fn) kdr_load_ibk(ctx, ibk_fn, 0);
	if(ibkd_fn) kdr_load_ibk(ctx, ibkd_fn, 1);
	
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
	mid = kdr_load_midi(ctx, midi_fn);
	if(!mid){
		if(alert("Couldn't load MIDI file.", "Maybe it is broken.", "Let's try again.", "Yes, please", "I am an idiot", 'y', 'n')){
			goto filesel;
		}
	}
	
	crc32_state_t crc;
	crc32_init(crc);
	crc32_buffer((char *) &mid->divisions, sizeof(mid->divisions), crc);
	for(int i = 0; i < KDR_MIDI_TRACKS; i++){
		if(mid->track[i].data && mid->track[i].len){
			crc32_buffer((char *) mid->track[i].data, mid->track[i].len, crc);
		}
	}
	printf("%x %x\n", crc[0], crc[1]);
	
	clear(screen);
	
	//Calculate MIDI duration
	kdr_get_midi_length(ctx, mid);
	nbeats = -ctx->midi_pos;
	nsecs = ctx->midi_time;
	
	//Initialize some stuff for WAV file capture
	int wavbuf_siz = (nsecs + 1) * sampling_rate * (1 + stereo), wavlen = 0;
	uint16_t *wavbuf = malloc(sizeof(*wavbuf) * wavbuf_siz);
	
	//Start playing
	kdr_play_midi(ctx, mid, 1);
	
	if(firstseek){
		kdr_midi_seek(ctx, firstseek);
	}
	
	rectfill(screen, 0, 100, SCREEN_W, 150, makecol(0, 0, 255));
	
	//Main loop
	while(!key[KEY_ESC] && !(close && (ctx->midi_time >= nsecs || ctx->midi_time >= maxsecs))){
		//START OF PLAYER UI CODE -----------------------------------------
		#if 1
		int cbeats, csecs, bar;
		int seekto, seek = 0;
		int multi = 1;
		cbeats = ctx->midi_pos;
		csecs = ctx->midi_time;
		
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
				kdr_midi_seek(ctx, seekto);
			}
			if(key[KEY_SPACE] && !oldsp){
				paused = 1;
				kdr_midi_pause(ctx);
			}
		}
		else{
			if(key[KEY_SPACE] && !oldsp){
				paused = 0;
				kdr_midi_resume(ctx);
			}
		}
		oldsp = key[KEY_SPACE];
		#endif
		//END OF PLAYER UI CODE -------------------------------------------
		
		//Use get_audio_stream_buffer to try and get a new buffer to fill with
		//n=audio_buf_siz samples
		if(wavonly || (audio_buf = (uint16_t *) get_audio_stream_buffer(audio_stream))){
			//Cut update into small updates to achieve good MIDI resolution
			int audio_buf_siz2 = audio_buf_siz;
			while((audio_buf_siz2 % 1 == 0) && (audio_buf_siz2 > sampling_rate / 100)) audio_buf_siz2 /= 2;
			for(uint16_t *audio_buf2 = audio_buf; audio_buf2 < audio_buf + audio_buf_siz * (stereo + 1); audio_buf2 += audio_buf_siz2 * (stereo + 1)){
				ymfm_generate(audio_buf2, audio_buf_siz2);
				kdr_update_midi(ctx, audio_buf_siz2, sampling_rate);
			}
			
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
	if(wavonly){
		char wav_fn[256];
		strcpy(wav_fn, get_filename(midi_fn));
		*(get_extension(wav_fn) - 1) = 0;
		strcat(wav_fn, YMFMLIB_USE_LIBRESAMPLE ? "_libresample" : "");
		strcat(wav_fn, ".wav");
		SAMPLE *wav_smpl = create_sample(16, stereo, sampling_rate, wavlen / (1 + stereo));
		memcpy(wav_smpl->data, wavbuf, wavlen * sizeof(*wavbuf));
		save_wav(wav_fn, wav_smpl);
		destroy_sample(wav_smpl);
		free(wavbuf);
	}
	
	end:
	allegro_exit();
	printf("Bye, MWF. Come back soon.\n");
	return 0;
}
END_OF_MAIN()
