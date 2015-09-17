#ifndef _SOUNDPLAY_H_
#define _SOUNDPLAY_H_

// #define DEBUG

#ifndef SOUNDQUEUEDEPTH
#define SOUNDQUEUEDEPTH 2
#endif // SOUNDQUEUEDEPTH

#define SOUND_FORMAT_BTC 1	// 1-bit sound (see http://www.romanblack.com/picsound.htm)
#define SOUND_FORMAT_BTC2 2	// 1.5bit / 2-pin BTC sound
#define SOUND_FORMAT_PCM 3 	// standard 8kHz, 8bit PCM


#ifdef SAMPLE_RATE
#define MAXCNTRELOAD F_CPU / 255 / 8 / SAMPLE_RATE
#else
#define MAXCNTRELOAD 255
#endif

#include <stdint.h>

extern volatile uint16_t samplepos;
extern volatile uint8_t sound_is_playing;

struct soundqueue_item_t
{
	uint16_t sounddata_p;
	uint16_t soundlen;
	uint8_t format;
	uint8_t speed;
	void (*finishfunc)(uint8_t);
	uint8_t finishparam;
	uint16_t samplepos;
};

extern struct soundqueue_item_t soundqueue[SOUNDQUEUEDEPTH];
extern uint8_t soundqueuecount, soundqueueindex;

#ifdef DEBUG
#define debugprint(i) Serial.print(#i": "); Serial.println(i);
#else // DEBUG
#define debugprint(i)	{}
#endif // DEBUG

void soundplayer_stop();

void finishplay_gotoprev();

void finishplay_gotoprev(uint8_t ignoredvar);

void finishplay_goto(uint8_t gotoindex);

void finishplay_durationds(uint8_t ds); // finish after ds/10 sec

void finishplay_repeat(uint8_t repeat);

void soundplayer_setup();

uint8_t soundplayer_play(uint16_t sounddata_p, uint16_t soundlen, uint8_t format, uint8_t speed,
		void (*finishfunc)(uint8_t), uint8_t finishparam);

uint8_t soundplayer_play(uint16_t sounddata_p, uint16_t soundlen, uint8_t format);

uint8_t soundplayer_play_repeat(uint16_t sounddata_p, uint16_t soundlen, uint8_t format,
		uint8_t repeat);

uint8_t soundplayer_play_ds(uint16_t sounddata_p, uint16_t soundlen, uint8_t format, uint8_t ds);

#endif // _SOUNDPLAY_H_
