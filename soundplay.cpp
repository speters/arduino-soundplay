//#define __AVR_ATmega328P__

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#ifndef  __AVR_ATmega328P__
#error "This code currently only runs on ATmega328	"
#endif
#include <Arduino.h>

#ifdef SOUNDFORMAT_PCM
#undef SOUNDFORMAT_BTC
#else
#ifndef SOUNDFORMAT_BTC
#error "Must define a sound format (SOUNDFORMAT_PCM|SOUNDFORMAT_BTC) in your Makefile"
#endif // SOUNDFORMAT_BTC
#endif // SOUNDFORMAT_PCM

#include "soundplay.h"

struct soundqueue_item_t soundqueue[SOUNDQUEUEDEPTH];
uint8_t soundqueuecount = 0, soundqueueindex = 0;
volatile uint16_t samplepos = 0;
volatile uint8_t sound_is_playing;

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
volatile uint8_t bitpos = 0;
volatile uint8_t sample;

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
				bitpos = 0;
			}
			else
			{
				soundqueue[soundqueueindex].finishfunc(
						soundqueue[soundqueueindex].finishparam);
				bitpos = 0;
				samplepos = 0;
				goto TIMER2_OVF_vect_SOUNDSTUFF_DONE;
				// TODO: Think about cleaner vs. shorter code...
			}
		}

		// TODO: 1.5bit implementation using two pins
		// toggle single pin according to current high bit
		if (sample & 0x80)
		{
			// PORTD |= (1 << PD3);
			digitalWrite(3, HIGH); // TODO: check if this is fast enough
		}
		else
		{
			// PORTD &= ~(1 << PD3);
			digitalWrite(3, LOW); // TODO: check if this is fast enough
		}

		sample <<= 1;
	}
	TIMER2_OVF_vect_SOUNDSTUFF_DONE: ;
}
#endif // SOUNDFORMAT_BTC

void soundplayer_setup()
{
	pinMode(3, OUTPUT);

	// setup clock
	ASSR &= ~(_BV(EXCLK) | _BV(AS2));

#ifndef SOUNDFORMAT_BTC
	TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
	TCCR2B = _BV(WGM22) | _BV(CS20);	// No prescaler
#else // SOUNDFORMAT_BTC
	TCCR2A = _BV(WGM21) | _BV(WGM20);
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

