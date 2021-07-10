#ifndef KDR_ADLIB_H

#define MIDI_OPL2             AL_ID('O','P','L','2')
#define MIDI_2XOPL2           AL_ID('O','P','L','X')
#define MIDI_OPL3             AL_ID('O','P','L','3')

extern MIDI_DRIVER midi_opl3, midi_opl2, midi_2xopl2;

static inline void install_opl_midi(int id)
{
	switch(id){
		case MIDI_OPL2:
			midi_driver = &midi_opl2;
			midi_card = id;
			break;
		case MIDI_OPL3:
			midi_driver = &midi_opl3;
			midi_card = id;
			break;
		case MIDI_2XOPL2:
			midi_driver = &midi_2xopl2;
			midi_card = id;
			break;
		default:
			return;
	}
	midi_driver->init(0, midi_driver->voices);
}

#endif
