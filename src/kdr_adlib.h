#ifndef KDR_ADLIB_H

#define KDR_MIDI_OPL2             AL_ID('O','P','L','2')
#define KDR_MIDI_2XOPL2           AL_ID('O','P','L','X')
#define KDR_MIDI_OPL3             AL_ID('O','P','L','3')

extern MIDI_DRIVER kdr_midi_opl3, kdr_midi_opl2, kdr_midi_2xopl2;

void kdr_install_opl_midi(int id);
int kdr_load_ibk(const char *filename, int drums);

#endif
