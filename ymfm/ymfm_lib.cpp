//START OF CODE FROM vgmrender.cpp -----------------------------------------------------------
#if 1

#define _CRT_SECURE_NO_WARNINGS

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <list>
#include <string>

#include "ymfm_misc.h"
#include "ymfm_opl.h"
#include "ymfm_opm.h"
#include "ymfm_opn.h"

#define LOG_WRITES (0)

// enable this to run the nuked OPN2 core in parallel; output is not captured,
// but logging can be added to observe behaviors
#define RUN_NUKED_OPN2 (0)
#if (RUN_NUKED_OPN2)
namespace nuked {
#include "test/ym3438.h"
}
#endif

// enable this to capture each chip at its native rate as well
#define CAPTURE_NATIVE (0 || RUN_NUKED_OPN2)


//*********************************************************
//  GLOBAL TYPES
//*********************************************************

// we use an int64_t as emulated time, as a 32.32 fixed point value
using emulated_time = int64_t;

// enumeration of the different types of chips we support
enum chip_type
{
	CHIP_YM2149,
	CHIP_YM2151,
	CHIP_YM2203,
	CHIP_YM2413,
	CHIP_YM2608,
	CHIP_YM2610,
	CHIP_YM2612,
	CHIP_YM3526,
	CHIP_Y8950,
	CHIP_YM3812,
	CHIP_YMF262,
	CHIP_YMF278B,
	CHIP_TYPES
};



//*********************************************************
//  CLASSES
//*********************************************************

// ======================> vgm_chip_base

// abstract base class for a Yamaha chip; we keep a list of these for processing
// as new commands come in
class vgm_chip_base
{
public:
	// construction
	vgm_chip_base(uint32_t clock, chip_type type, char const *name) :
		m_type(type),
		m_name(name)
	{
	}

	// simple getters
	chip_type type() const { return m_type; }
	virtual uint32_t sample_rate() const = 0;

	// required methods for derived classes to implement
	virtual void write(uint32_t reg, uint8_t data) = 0;
	virtual void generate(emulated_time output_start, emulated_time output_step, int32_t *buffer) = 0;

	// write data to the ADPCM-A buffer
	void write_data(ymfm::access_class type, uint32_t base, uint32_t length, uint8_t const *src)
	{
		uint32_t end = base + length;
		if (end > m_data[type].size())
			m_data[type].resize(end);
		memcpy(&m_data[type][base], src, length);
	}

	// seek within the PCM stream
	void seek_pcm(uint32_t pos) { m_pcm_offset = pos; }
	uint8_t read_pcm() { auto &pcm = m_data[ymfm::ACCESS_PCM]; return (m_pcm_offset < pcm.size()) ? pcm[m_pcm_offset++] : 0; }

protected:
	// internal state
	chip_type m_type;
	std::string m_name;
	std::vector<uint8_t> m_data[ymfm::ACCESS_CLASSES];
	uint32_t m_pcm_offset;
#if (CAPTURE_NATIVE)
public:
	std::vector<int32_t> m_native_data;
#endif
#if (RUN_NUKED_OPN2)
public:
	nuked::ym3438_t *m_external = nullptr;
	std::vector<int32_t> m_nuked_data;
#endif
};


// ======================> vgm_chip

// actual chip-specific implementation class; includes implementatino of the
// ymfm_interface as needed for vgmplay purposes
template<typename ChipType>
class vgm_chip : public vgm_chip_base, public ymfm::ymfm_interface
{
public:
	// construction
	vgm_chip(uint32_t clock, chip_type type, char const *name) :
		vgm_chip_base(clock, type, name),
		m_chip(*this),
		m_clock(clock),
		m_clocks(0),
		m_step(0x100000000ull / m_chip.sample_rate(clock)),
		m_pos(0)
	{
		m_chip.reset();
#if (RUN_NUKED_OPN2)
		if (type == CHIP_YM2612)
		{
			m_external = new nuked::ym3438_t;
			nuked::OPN2_SetChipType(nuked::ym3438_mode_ym2612);
			nuked::OPN2_Reset(m_external);
		}
#endif
	}

	virtual uint32_t sample_rate() const override
	{
		return m_chip.sample_rate(m_clock);
	}

	// handle a register write: just queue for now
	virtual void write(uint32_t reg, uint8_t data) override
	{
		m_queue.push_back(std::make_pair(reg, data));
	}

	// generate one output sample of output
	virtual void generate(emulated_time output_start, emulated_time output_step, int32_t *buffer) override
	{
		uint32_t addr1 = 0xffff, addr2 = 0xffff;
		uint8_t data1 = 0, data2 = 0;

		// see if there is data to be written; if so, extract it and dequeue
		if(!m_queue.empty())
		{
			auto front = m_queue.front();
			addr1 = 0 + 2 * ((front.first >> 8) & 3);
			data1 = front.first & 0xff;
			addr2 = addr1 + ((m_type == CHIP_YM2149) ? 2 : 1);
			data2 = front.second;
			m_queue.erase(m_queue.begin());
		}

		// write to the chip
		if (addr1 != 0xffff)
		{
			if (LOG_WRITES)
				printf("%10.5f: %s %03X=%02X\n", double(m_clocks) / double(m_chip.sample_rate(m_clock)), m_name.c_str(), data1, data2);
			m_chip.write(addr1, data1);
			m_chip.write(addr2, data2);
		}

		// generate at the appropriate sample rate
		for ( ; m_pos <= output_start; m_pos += m_step)
		{
			m_chip.generate(&m_output);

#if (CAPTURE_NATIVE)
			// if capturing native, append each generated sample
			m_native_data.push_back(m_output.data[0]);
			m_native_data.push_back(m_output.data[ChipType::OUTPUTS > 1 ? 1 : 0]);
#endif

#if (RUN_NUKED_OPN2)
			// if running nuked, capture its output as well
			if (m_external != nullptr)
			{
				int32_t sum[2] = { 0 };
				if (addr1 != 0xffff)
					nuked::OPN2_Write(m_external, addr1, data1);
				nuked::Bit16s buffer[2];
				for (int clocks = 0; clocks < 12; clocks++)
				{
					nuked::OPN2_Clock(m_external, buffer);
					sum[0] += buffer[0];
					sum[1] += buffer[1];
				}
				if (addr2 != 0xffff)
					nuked::OPN2_Write(m_external, addr2, data2);
				for (int clocks = 0; clocks < 12; clocks++)
				{
					nuked::OPN2_Clock(m_external, buffer);
					sum[0] += buffer[0];
					sum[1] += buffer[1];
				}
				addr1 = addr2 = 0xffff;
				m_nuked_data.push_back(sum[0] / 24);
				m_nuked_data.push_back(sum[1] / 24);
			}
#endif
		}

		// add the final result to the buffer
		if (m_type == CHIP_YM2203)
		{
			int32_t out0 = m_output.data[0];
			int32_t out1 = m_output.data[1 % ChipType::OUTPUTS];
			int32_t out2 = m_output.data[2 % ChipType::OUTPUTS];
			int32_t out3 = m_output.data[3 % ChipType::OUTPUTS];
			*buffer++ += out0 + out1 + out2 + out3;
			*buffer++ += out0 + out1 + out2 + out3;
		}
		else if (m_type == CHIP_YM2608 || m_type == CHIP_YM2610)
		{
			int32_t out0 = m_output.data[0];
			int32_t out1 = m_output.data[1 % ChipType::OUTPUTS];
			int32_t out2 = m_output.data[2 % ChipType::OUTPUTS];
			*buffer++ += out0 + out2;
			*buffer++ += out1 + out2;
		}
		else if (m_type == CHIP_YMF278B)
		{
			*buffer++ += m_output.data[4];
			*buffer++ += m_output.data[5];
		}
		else if (ChipType::OUTPUTS == 1)
		{
			*buffer++ += m_output.data[0];
			*buffer++ += m_output.data[0];
		}
		else
		{
			*buffer++ += m_output.data[0];
			*buffer++ += m_output.data[1 % ChipType::OUTPUTS];
		}
		m_clocks++;
	}

protected:
	// handle a read from the buffer
	virtual uint8_t ymfm_external_read(ymfm::access_class type, uint32_t offset) override
	{
		auto &data = m_data[type];
		return (offset < data.size()) ? data[offset] : 0;
	}

	// internal state
	ChipType m_chip;
	uint32_t m_clock;
	uint64_t m_clocks;
	typename ChipType::output_data m_output;
	emulated_time m_step;
	emulated_time m_pos;
	std::vector<std::pair<uint32_t, uint8_t>> m_queue;
};

#endif
//END OF CODE FROM vgmrender.cpp -------------------------------------------------------------

//Hacks so we can link with gcc rather than g++
#if 1
extern "C" void __cxa_pure_virtual()
{
	abort();
}
void operator delete(void* p, unsigned int i)
{
}
void operator delete(void* p, unsigned long i)
{
}
#endif

//Provide QueryPerformanceCounter
#include <windows.h>

#include "../src/ymfm_lib.h"

#if YMFMLIB_USE_LIBRESAMPLE
#include "../libresample/include/libresample.h"
#endif

struct reg_write_t
{
	int reg;
	unsigned char data;
};

static unsigned int output_rate, chip_rate;
static vgm_chip_base *chip;
static std::vector<reg_write_t> reg_writes;
static int stereo;
static std::vector<float> f_buffer[2];
static void *resampler[2];

void ymfm_init(unsigned int clock, unsigned int output_rate_, int stereo_)
{
	stereo = stereo_;
	output_rate = output_rate_;
	chip = new vgm_chip<ymfm::ymf262>(clock, CHIP_YMF262, "YMF262");
	chip_rate = chip->sample_rate();
	
	#if YMFMLIB_USE_LIBRESAMPLE
	//Initialize resamplers (one for each channel)
	for(int i = 0; i < 2; i++) resampler[i] = resample_open(
		1,                                   //highQuality
		double(44100/5) / double(chip_rate), //minFactor
		double(48000) / double(chip_rate)    //maxFactor
	);
	#endif
}

void ymfm_write(int reg, unsigned char data)
{
	reg_write_t rw = {reg, data};
	reg_writes.push_back(rw);
}

void ymfm_generate(void *buffer, int num_samples)
{
	emulated_time output_step = 0x100000000ull / chip_rate;
	static emulated_time output_pos = 0;
	
	//Number of samples to generate from OPL (not counting stereo)
	int num_samples2 = (num_samples * chip_rate) / output_rate;
	
	//u16buffer will hold output samples,
	//u16buffer2 will hold generated samples if not using libresample
	static int u16buffer2_siz = 0;
	static uint16_t *u16buffer2 = 0;
	uint16_t *u16buffer = (uint16_t *) buffer;
	//Reallocate if small
	if(u16buffer2_siz < num_samples2 * 2){
		u16buffer2_siz = num_samples2 * 2;
		u16buffer2 = (uint16_t *) realloc((void *) u16buffer2, sizeof(*u16buffer2) * u16buffer2_siz);
	}
	
	//Write in registers
	while(!reg_writes.empty()){
		reg_write_t front = reg_writes.front();
		chip->write(front.reg, front.data);
		reg_writes.erase(reg_writes.begin());
	}
	
	for(int i = 0; i < num_samples2; i++){
		int32_t outputs[2] = {0};
		chip->generate(output_pos, output_step, outputs);
		output_pos += output_step;
		
		#ifdef YMFMLIB_USE_LIBRESAMPLE
		//Convert samples to float and add to resampling buffer
		           f_buffer[0].push_back(float(outputs[0]) / float(1 << 15));
		if(stereo) f_buffer[1].push_back(float(outputs[1]) / float(1 << 15));
		#endif
		
		//Convert samples to unsigned and clip
		outputs[0] += 32768;
		outputs[0] &= 0xFFFF;
		outputs[1] += 32768;
		outputs[1] &= 0xFFFF;
		
		u16buffer2[i*2+0] = outputs[0];
		u16buffer2[i*2+1] = outputs[1];
	}
	
	//Perform resampling
	#if YMFMLIB_USE_LIBRESAMPLE
	//Use libresample to downsample each channel separately
	static float *out_f_buffer = 0;
	if(!out_f_buffer){
		out_f_buffer = (float *) malloc(sizeof(*out_f_buffer) * (num_samples));
	}
	for(int i = 0; i < (stereo + 1); i++){
		int in_used = 0;
		int out_used = resample_process(
						resampler[i],                              //handle
						(double) output_rate / (double) chip_rate, //factor
						&f_buffer[i][0],                           //inBuffer
						num_samples2,                              //inBufferLen
						0,                                         //lastFlag
						&in_used,                                  //inUsed
						out_f_buffer,                              //outBuffer
						num_samples                                //outBufferLen
		);
		//Pad out_f_buffer
		for(int j = out_used; j < num_samples; j++){
			out_f_buffer[j] = out_f_buffer[out_used - 1];
		}
		//Convert back to u16
		for(int j = 0; j < num_samples; j++){
			u16buffer[j*(stereo+1)+i] = (out_f_buffer[j] + 0.5) * 32768;
		}
		//Remove used samples
		f_buffer[i].erase(f_buffer[i].begin(), f_buffer[i].begin() + in_used);
	}
	#else
	//Pointless aliased "downsampler"
	for(int i = 0; i < num_samples; i++){
		int pos = (i * num_samples2) / num_samples;
		
		if(stereo){
			u16buffer[i*2+0] = u16buffer2[pos*2+0];
			u16buffer[i*2+1] = u16buffer2[pos*2+1];
		}
		else{
			u16buffer[i] = u16buffer2[pos*2];
		}
	}
	#endif
}
