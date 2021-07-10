# Allegro 4 MIDI driver for YMFM OPL3 emulator #

This is an attempt to use [YMFM](https://github.com/aaronsgiles/ymfm/) to emulate the OPL3 in a Sound Blaster 16, while using a modified version of the Allegro 4 Sound Blaster 16 MIDI driver for MS-DOS to generate the register writes.

It seems to be partially successful, but there are some timing issues.

My own bits of the code clearly shows that I am a C rather than C++ person.

Much of the YMFM "frontend" code was taken from [vgmrender.cpp](https://github.com/aaronsgiles/ymfm/blob/ef21f08a16f44b005c9ace5f8f44ae6f95dbf3f0/examples/vgmrender/vgmrender.cpp).

The "wavs" folder contains rendered wavs, to compare with DOSBox captures also there.

## Issues ##

The original DOS code would use a [midi_player routine](https://github.com/msikma/allegro-4.2.2-xc/blob/ecd87af7f43f9e08f49acc78f4901c39221dbab5/src/midi.c#L897) triggered regularly by a PIT timer interrupt to directly call other routines such as [a key_on handler](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/src/kdr_adlib.c#L525).

Meanwhile, the emulator needs to run as it is triggered by an available audio buffer. Thus I implemented a timing of register writes using a high resolution timer to simulate registers being written to on the fly. [Writes are timed and added to a queue](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/ymfm/ymfm_lib.cpp#L321) and then written at the correct sample timing. This seems to make a difference, even with low buffer size:

![Spectrogram without timing](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/wavs/endless.wav.png?raw=true "Without timing")

![Spectrogram with timing](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/wavs/endless_time.wav.png?raw=true "With timing")

Compare with DOSBox though (note, not exactly same scale) probably timing issues remain:

![Spectrogram from DOSBox](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/wavs/endless_dosbox.wav.png?raw=true "From DOSBox")

There is also [an issue with sampling rate mentioned in YMFM readme](https://github.com/aaronsgiles/ymfm/blob/ef21f08a16f44b005c9ace5f8f44ae6f95dbf3f0/README.md#clocking) which I don't entirely understand (i.e. I don't understand whether running at the lower rate will be "wrong", like missing register writes etc, or just aliased). Seems the output rate for the OPL3 on a Sound Blaster 16 as calculated by sample_rate() method is 49715 Hz. I have added a "downsampler" (does a straight aliased copy), but this seems to have an almost identical result to running the generator at the lower rate (obviously a proper FFT downsampler would be better). Check spectrogram comparison:

![Spectrogram without resampling](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/wavs/endless_time.wav.png?raw=true "Without resampling")

![Spectrogram with resampling](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/wavs/endless_rsmpl_time.wav.png?raw=true "With resampling")

## Places of interest ##

All the register writes go through [this function](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/src/kdr_adlib.c#L243)
```c
/* fm_write:
 *  Writes a byte to the specified register on the FM chip.
 */
//MODIFIED TO USE ymfm RATHER THAN MS-DOS outportb
static void fm_write(int reg, unsigned char data)
{
   ymfm_write(reg, data);
}
END_OF_STATIC_FUNCTION(fm_write);
```

There exist two important flags at [this header](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/src/ymfm_lib.h#L6)
```c
#define YMFMLIB_RESAMPLE        1
#define YMFMLIB_TIME_REG_WRITES 1
```

And the meat of [the YMFM "frontend" is here](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/ymfm/ymfm_lib.cpp#L329)
```cpp
void ymfm_generate(void *buffer, int num_samples)
{
  \\ ...
```

## C++ to C hack ##

The build is set up so the YMFM side can be built with g++, but then the program can be linked with gcc.
