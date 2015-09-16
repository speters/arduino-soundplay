#define DEBUG
#include <stdint.h>
// sample tables made with wav2c https://github.com/olleolleolle/wav2c , must be in 8bit mono format
#include "sound_crankup.h"
#include "sound_diesel.h"
#include "sound_hupe.h"

// if sample reate is not 7843Hz, then adjust it here:
// #define SAMPLE_RATE 8000

#ifndef SOUNDQUEUEDEPTH
#define SOUNDQUEUEDEPTH 2
#endif // SOUNDQUEUEDEPTH

//#define ___AVR_ATmega328P__
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <Arduino.h>

#define SOUNDFORMAT_BTC // 1-bit sound (see http://www.romanblack.com/picsound.htm)

#ifdef SAMPLE_RATE
#define MAXCNTRELOAD F_CPU / 255 / 8 / SAMPLE_RATE
#else
#ifndef SOUNDFORMAT_BTC
#define MAXCNTRELOAD 255
#else // SOUNDFORMAT_BTC
#define MAXCNTRELOAD 127
#endif // SOUNDFORMAT_BTC
#endif

volatile uint16_t samplepos = 0;
volatile uint8_t sound_is_playing;

struct soundqueue_item_t
{
	uint16_t sounddata_p;
	uint16_t soundlen;
	uint16_t samplepos;
	uint8_t speed;
	void (*finishfunc)(uint8_t);
	uint8_t finishparam;
};

struct soundqueue_item_t soundqueue[SOUNDQUEUEDEPTH];
uint8_t soundqueuecount = 0, soundqueueindex = 0;

#ifdef DEBUG
#define debugprint(i) Serial.print(#i": "); Serial.println(i);
#else // DEBUG
#define debugprint(i)	{}
#endif // DEBUG

void soundplayer_stop()
{
#ifndef SOUNDFORMAT_BTC
	// disable interrupt
	TIMSK2 &= ~(_BV(TOIE2));
#else // SOUNDFORMAT_BTC
	sound_is_playing = 0;
#endif // SOUNDFORMAT_BTC

	soundqueuecount = 0;
	digitalWrite(3, LOW);
}

void finishplay_gotoprev()
{
	if (soundqueuecount > 0)
	{
		--soundqueuecount;
		if (soundqueuecount == 0)
		{
			soundqueueindex = 0;
		}
		else
		{
			soundqueueindex = soundqueuecount - 1;
		}
	}
	else
	{
		soundqueueindex = 0;
		soundplayer_stop();
	}
}

void finishplay_gotoprev(uint8_t ignoredvar)
{
	finishplay_gotoprev();
}

void finishplay_goto(uint8_t gotoindex)
{
	if (gotoindex >= 0 && gotoindex < SOUNDQUEUEDEPTH)
	{
		soundqueueindex = gotoindex;
	}
	else
	{
		gotoindex = 0;
	}
}

void finishplay_durationds(uint8_t ds) // finish after ds/10 sec
{
	static uint32_t ms = 0;
	if (ms == 0)
	{
		ms = millis() + (ds * 100);
	}
	else
	{
		if (millis() > ms)
		{
			ms = 0;
			finishplay_gotoprev();
		}
	}
}

void finishplay_repeat(uint8_t repeat)
{
	static uint8_t r = 0;
	if (repeat == 0)
	{
		r = 0;
	}
	else
	{
		r++;
		if (r >= repeat)
		{
			r = 0;
			finishplay_gotoprev();
		}
	}
}

#ifndef SOUNDFORMAT_BTC
uint8_t nth = 0;

ISR(TIMER2_OVF_vect)
{
	if (nth > 0)
	{
		if (nth == 7)
		{
			OCR2A = soundqueue[soundqueueindex].speed;
		}
		--nth;
	}
	else
	{
		nth = 7;

		if (samplepos >= soundqueue[soundqueueindex].soundlen)
		{
			samplepos = 0;
			soundqueue[soundqueueindex].finishfunc(
					soundqueue[soundqueueindex].finishparam);
		}

		OCR2B = pgm_read_byte(
				soundqueue[soundqueueindex].sounddata_p + samplepos);
		++samplepos;
	}
}
#else // SOUNDFORMAT_BTC
uint8_t bitpos;
uint8_t sample;

ISR(TIMER2_OVF_vect)
{
	if (sound_is_playing)
	{
		// Load current "speed" - needed for e.g. adjusting the sound pitch of an engine sample
		OCR2A = soundqueue[soundqueueindex].speed;
		if (++bitpos & 8)
		{
			if (samplepos < soundqueue[soundqueueindex].soundlen)
			{
				// load next byte with 1-bit samples
				sample = pgm_read_byte(
						soundqueue[soundqueueindex].sounddata_p + samplepos);
				samplepos++;
			}
			else
			{
				soundqueue[soundqueueindex].finishfunc(
						soundqueue[soundqueueindex].finishparam);
				goto TIMER2_OVF_vect_SOUNDSTUFF_DONE;
				// TODO: Think about cleaner vs. shorter code...
			}
			bitpos = 0;
		}

		// toggle single pin according to current high bit
		if (sample & 0x80)
		{
			// PORTD |= (1 << PD3);
			digitalWrite(3, HIGH);
		}
		else
		{
			// PORTD &= ~(1 << PD3);
			digitalWrite(3, LOW);
		}
	}
	TIMER2_OVF_vect_SOUNDSTUFF_DONE: ;
}
#endif // SOUNDFORMAT_BTC

void soundplayer_setup()
{
	pinMode(3, OUTPUT);

	// use internal clock (datasheet p.160)
	ASSR &= ~(_BV(EXCLK) | _BV(AS2));

	TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);

#ifndef SOUNDFORMAT_BTC
	TCCR2B = _BV(WGM22) | _BV(CS20);	// No prescaler
#else // SOUNDFORMAT_BTC
	TCCR2B = _BV(WGM22) | _BV(CS21);	// /8 prescaler
#endif

	OCR2A = MAXCNTRELOAD;
}

uint8_t soundplayer_play(uint16_t sounddata_p, uint16_t soundlen, uint8_t speed,
		void (*finishfunc)(uint8_t), uint8_t finishparam)
{
	TIMSK2 &= ~(_BV(TOIE2));

	if (soundqueuecount > 0)
	{
		if (soundqueuecount < 0xff)
		{
			++soundqueuecount;
		}
		if (soundqueuecount > SOUNDQUEUEDEPTH)
		{
			soundqueuecount = SOUNDQUEUEDEPTH;
		}
		soundqueueindex = soundqueuecount - 1;
		if (soundqueueindex > 1)
		{
			// save current speed
			soundqueue[soundqueueindex - 1].speed = OCR2A;
			// save current position
			soundqueue[soundqueueindex - 1].samplepos = samplepos;
		}
	}
	else
	{
		soundqueuecount = 1;
		soundqueueindex = 0;
	}

	soundqueue[soundqueueindex].sounddata_p = sounddata_p;
	soundqueue[soundqueueindex].soundlen = soundlen;
	soundqueue[soundqueueindex].speed = speed;
	soundqueue[soundqueueindex].samplepos = 0;
	soundqueue[soundqueueindex].finishfunc = finishfunc;
	soundqueue[soundqueueindex].finishparam = finishparam;

	TIMSK2 = _BV(TOIE2);
	samplepos = 0;
	sound_is_playing = 1;
	sei();

	return soundqueueindex;
}

uint8_t soundplayer_play(uint16_t sounddata_p, uint16_t soundlen)
{
	return soundplayer_play(sounddata_p, soundlen, (uint8_t) MAXCNTRELOAD,
			finishplay_gotoprev, 0);
}

uint8_t soundplayer_play_repeat(uint16_t sounddata_p, uint16_t soundlen,
		uint8_t repeat)
{
	return soundplayer_play(sounddata_p, soundlen, (uint8_t) MAXCNTRELOAD,
			finishplay_repeat, repeat);
}

uint8_t soundplayer_play_ds(uint16_t sounddata_p, uint16_t soundlen, uint8_t ds)
{
	return soundplayer_play(sounddata_p, soundlen, (uint8_t) MAXCNTRELOAD,
			finishplay_durationds, ds);
}

uint8_t backgroundsoundindex, i;
void setup()
{
#ifdef DEBUG
	Serial.begin(115200);
#endif
	pinMode(A1, INPUT);

	soundplayer_setup();

	backgroundsoundindex = soundplayer_play((uint16_t) &sound_diesel_data,
			sound_diesel_length, MAXCNTRELOAD, finishplay_repeat, 0);

	soundplayer_play_repeat((uint16_t) &sound_crankup_data,
			sound_crankup_length, 3);
	//soundplayer_play((uint16_t) &sound_crankup_data, sound_crankup_length, MAXCNTRELOAD>>1, finishplay_repeat, 3);
	/*
	 debugprint(backgroundsoundindex);
	 delay(2000);
	 i = soundplayer_play((uint16_t) &sound_hupe_data, sound_hupe_length,
	 MAXCNTRELOAD, finishplay_durationds, 30);
	 debugprint(i);
	 // delay(3000);
	 //startPlayback((uint16_t) &sounddata_data, sounddata_length, MAXCNTRELOAD, finishplay_hupe);
	 //delay(1000);
	 delay(4000);
	 debugprint(soundqueueindex);
	 */
}

void loop()
{
	uint16_t analogVal;

	while (true)
	{
		if (digitalRead (A2))
		{
			soundplayer_play_ds((uint16_t) &sound_hupe_data, sound_hupe_length,
					20);
		}
		if (soundqueueindex == backgroundsoundindex)
		{
			analogVal = analogRead(A7);
			soundqueue[backgroundsoundindex].speed = map(analogVal, 0, 1023,
			MAXCNTRELOAD - (255 - 170), MAXCNTRELOAD);
		}
	}
}
