# Allegro 4 MIDI driver for YMFM OPL3 emulator #

This is an attempt to use [YMFM](https://github.com/aaronsgiles/ymfm/) to emulate the OPL3 in a Sound Blaster 16, while using a modified version of the Allegro 4 Sound Blaster 16 MIDI driver for MS-DOS to generate the register writes.

It seems to be partially successful, but there are some timing issues.

My own bits of the code clearly shows that I am a C rather than C++ person.

Much of the YMFM "frontend" code was taken from [vgmrender.cpp](https://github.com/aaronsgiles/ymfm/blob/ef21f08a16f44b005c9ace5f8f44ae6f95dbf3f0/examples/vgmrender/vgmrender.cpp).

The "wavs" folder contains rendered wavs, to compare with DOSBox captures also there.

## Issues ##

The original DOS code would use a [midi_player routine](https://github.com/msikma/allegro-4.2.2-xc/blob/ecd87af7f43f9e08f49acc78f4901c39221dbab5/src/midi.c#L897) triggered regularly by a PIT timer interrupt to directly call other routines such as [a key_on handler](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/src/kdr_adlib.c#L525).

Meanwhile, the emulator needs to run as it is triggered by an available audio buffer. Thus I implemented a timing of register writes using a high resolution timer to simulate registers being written to on the fly. [Writes are timed and added to a queue](https://github.com/kdrnic/al4_ymfm_midi/blob/c784c03fee79bcf291c2b275743ef059be8392f5/ymfm/ymfm_lib.cpp#L321) and then written at the correct sample timing. This seems to make a difference, even with low buffer size. See a comparison:

![YMFM_TIME_REG_WRITES=0](https://github.com/kdrnic/al4_ymfm_midi/blob/34ddfa17036620d8afa5473d1caf92b3c0f3ffa6/comp2/endless_rsmpl_libresample.wav.png?raw=true "YMFM_TIME_REG_WRITES=0")

![YMFM_TIME_REG_WRITES=1](https://github.com/kdrnic/al4_ymfm_midi/blob/34ddfa17036620d8afa5473d1caf92b3c0f3ffa6/comp2/endless_rsmpl_time_libresample.wav.png?raw=true "YMFM_TIME_REG_WRITES=1")

Compare with DOSBox though ~~(note, not exactly same scale)~~(I have edited the images to scale) probably timing issues remain:

![Spectrogram from this program](https://github.com/kdrnic/al4_ymfm_midi/blob/13000c46ead01b6853c1ad6e61989d0174d7da1d/comparison/endless_rsmpl_time_libresample.wav.png?raw=true "This program")

![Spectrogram from DOSBox](https://github.com/kdrnic/al4_ymfm_midi/blob/13000c46ead01b6853c1ad6e61989d0174d7da1d/comparison/endless_dosbox.wav.png?raw=true "From DOSBox")

(I have now found that adding an Allegro rest(20) call, which similar to sleep(1) but with supposed ms resolution, fixes most timing issues. I now think the cause of all issues may be some absurd issue with timer resolution on Windows).

There is also [an issue with sampling rate mentioned in YMFM readme](https://github.com/aaronsgiles/ymfm/blob/ef21f08a16f44b005c9ace5f8f44ae6f95dbf3f0/README.md#clocking) which I don't entirely understand (i.e. I don't understand whether running at the lower rate will be "wrong", like missing register writes etc, or just aliased). Seems the output rate for the OPL3 on a Sound Blaster 16 as calculated by sample_rate() method is 49715 Hz. ~~I have added a "downsampler" (does a straight aliased copy), but this seems to have an almost identical result to running the generator at the lower rate (obviously a proper FFT downsampler would be better).~~ I have added downsampling with [libresample](https://github.com/minorninth/libresample). Check spectrogram comparison:

![Spectrogram without resampling](https://github.com/kdrnic/al4_ymfm_midi/blob/63bbf2cdfffbf9438fa499b9c9b90341a3659d25/wavs/nightcall_time.wav.png "Without resampling")

![Spectrogram with libresample](https://github.com/kdrnic/al4_ymfm_midi/blob/63bbf2cdfffbf9438fa499b9c9b90341a3659d25/wavs/nightcall_rsmpl_time_libresample.wav.png "With libresample")

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
