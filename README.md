# kdrnic's OPL3 MIDI player #

This is a library which uses [YMFM](https://github.com/aaronsgiles/ymfm/) to emulate the OPL3 in a Sound Blaster 16, while using a modified version of the Allegro 4 Sound Blaster 16 MIDI driver for MS-DOS to generate the register writes.

The Allegro 4 MIDI playing code was heavily  modified to be completely independent of Allegro 4 and no longer contains platform specific code.

Much of the YMFM "frontend" code was taken from [vgmrender.cpp](https://github.com/aaronsgiles/ymfm/blob/ef21f08a16f44b005c9ace5f8f44ae6f95dbf3f0/examples/vgmrender/vgmrender.cpp).

## Usage ##

A example of use is provided in main.c.

It uses Allegro 4 for user interface, but can be easily modified to be an Allegro 4 independent text-only MIDI to WAV renderer.

Additionally, a minimal sample is shown below. You might check out Allegro 4 documentation to get information on additional routines.

```c
uint16_t *get_audio_stream_buffer(...)
{
   //Returns a buffer to be filled...
}

void *ymfm = ymfm_init(YMFM_SB16_OPL_CLOCK_RATE, sampling_rate, stereo);
KDR_MIDI_CTX *ctx = kdr_create_midi_ctx();
kdr_install_driver(ctx, &kdr_midi_opl3, ymfm);
KDR_MIDI *mid = kdr_load_midi(ctx, midi_fn);
kdr_play_midi(ctx, mid, 0);
while(...){
   uint16_t *audio_buf = get_audio_stream_buffer(...);
   ymfm_generate(ymfm, audio_buf, num_samples);
   kdr_update_midi(ctx, num_samples, sampling_rate);
}
kdr_destroy_midi(ctx, mid);
ymfm_destroy(ymfm);
kdr_destroy_midi_ctx(ctx);
```

## Solved issues ##

### Interrupts ###

The original DOS code would use a [midi_player routine](https://github.com/msikma/allegro-4.2.2-xc/blob/ecd87af7f43f9e08f49acc78f4901c39221dbab5/src/midi.c#L897) triggered regularly by a PIT timer interrupt to directly call other routines such as [a key_on handler](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/src/kdr_adlib.c#L525).

Meanwhile, the emulator needs to run as it is triggered by an available audio buffer. Thus, I modified the code from being interrupt driven to one driven by a call to a user accessible update routine.

### Deglobalization ###

The original Allegro code was heavy on its use of globals. I made it so all the former globals now rest in a KDR_MIDI_CTX structure.

### Downsampling ###

There exists [an issue with sampling rate mentioned in YMFM readme](https://github.com/aaronsgiles/ymfm/blob/ef21f08a16f44b005c9ace5f8f44ae6f95dbf3f0/README.md#clocking) which I don't entirely understand (i.e. I don't understand whether running at the lower rate will be "wrong", like missing register writes etc, or just aliased). Seems the output rate for the OPL3 on a Sound Blaster 16 as calculated by sample_rate() method is 49715 Hz. I have added downsampling with [libresample](https://github.com/minorninth/libresample). I've checked spectrograms and this provides nice, unaliased sound.

### C++ to C hack ###

The build is set up so the YMFM side can be built with g++, but then the program can be linked with gcc.

This is done through some gcc flags and providing a cxa_pure_virtual and operator delete for void pointers.
