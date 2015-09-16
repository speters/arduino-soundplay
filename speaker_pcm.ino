#define DEBUG
#include <stdint.h>
// sample tables made with wav2c https://github.com/olleolleolle/wav2c , must be in 8bit ~8kHz format
#include "sound_crankup.h"
#include "sound_diesel.h"
#include "sound_hupe.h"

// if sample reate is not 7843Hz, then adjust it here:
// #define SAMPLE_RATE 8000

// if interrupt load is too high, comment out to go without oversampling
// #define NO_OVERSAMPLING

#ifndef SOUNDQUEUEDEPTH
#define SOUNDQUEUEDEPTH 2
#endif // SOUNDQUEUEDEPTH

#define ___AVR_ATmega328P__
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <Arduino.h>

#ifdef SAMPLE_RATE
#define MAXCNTRELOAD F_CPU / 255 / 8 / SAMPLE_RATE
#else
#define MAXCNTRELOAD 255
#endif

volatile uint16_t soundsample = 0;

struct soundqueue_item_t
{
	uint16_t sounddata_p;
	uint16_t soundlen;
	uint16_t sample;
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
	// disable interrupt
	TIMSK2 &= ~(_BV(TOIE2));

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

#ifndef NO_OVERSAMPLING
uint8_t nth = 0;
#endif // NO_OVERSAMPLING

ISR(TIMER2_OVF_vect)
{
#ifndef NO_OVERSAMPLING
	if (nth > 0)
	{
		if (nth == 7)
		{
			OCR2A = soundqueue[soundqueueindex].speed;
		}
		--nth;
		return;
	}
	nth = 7;
#endif // NO_OVERSAMPLING

	if (soundsample >= soundqueue[soundqueueindex].soundlen)
	{
		soundsample = 0;
		soundqueue[soundqueueindex].finishfunc(
				soundqueue[soundqueueindex].finishparam);
	}

	OCR2B = pgm_read_byte(
			soundqueue[soundqueueindex].sounddata_p + soundsample);
	++soundsample;
}

void soundplayer_setup()
{
	pinMode(3, OUTPUT);

	// use internal clock (datasheet p.160)
	ASSR &= ~(_BV(EXCLK) | _BV(AS2));

	TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);

#ifndef NO_OVERSAMPLING
	TCCR2B = _BV(WGM22) | _BV(CS20);	// No prescaler
#else
			TCCR2B = _BV(WGM22)| _BV(CS21);	// /8 prescaler
#endif // NO_OVERSAMPLING

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
			soundqueue[soundqueueindex - 1].sample = soundsample;
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
	soundqueue[soundqueueindex].sample = 0;
	soundqueue[soundqueueindex].finishfunc = finishfunc;
	soundqueue[soundqueueindex].finishparam = finishparam;

	TIMSK2 = _BV(TOIE2);
	soundsample = 0;
	sei();

	return soundqueueindex;
}

uint8_t soundplayer_play(uint16_t sounddata_p, uint16_t soundlen)
{
	return soundplayer_play(sounddata_p, soundlen, (uint8_t) MAXCNTRELOAD, finishplay_gotoprev, 0);
}

uint8_t soundplayer_play_repeat(uint16_t sounddata_p, uint16_t soundlen, uint8_t repeat)
{
	return soundplayer_play(sounddata_p, soundlen, (uint8_t) MAXCNTRELOAD, finishplay_repeat, repeat);
}

uint8_t soundplayer_play_ds(uint16_t sounddata_p, uint16_t soundlen, uint8_t ds)
{
	return soundplayer_play(sounddata_p, soundlen, (uint8_t) MAXCNTRELOAD, finishplay_durationds, ds);
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

	soundplayer_play_repeat((uint16_t) &sound_crankup_data, sound_crankup_length, 3);
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
		if (digitalRead(A2))
		{
			soundplayer_play_ds((uint16_t) &sound_hupe_data, sound_hupe_length, 20);
		}
		if (soundqueueindex == backgroundsoundindex)
		{
			analogVal = analogRead(A7);
			soundqueue[backgroundsoundindex].speed = map(analogVal, 0, 1023,
					170, MAXCNTRELOAD);
		}
	}
}
