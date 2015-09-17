//#define __AVR_ATmega328P__

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#ifndef  __AVR_ATmega328P__
#error "This code currently only runs on ATmega328	"
#endif
#include <Arduino.h>

#include "soundplay.h"

struct soundqueue_item_t soundqueue[SOUNDQUEUEDEPTH];
uint8_t soundqueuecount = 0, soundqueueindex = 0;
volatile uint16_t samplepos = 0;
volatile uint8_t sound_is_playing;

void soundplayer_stop()
{
	// disable interrupt
	// TIMSK2 &= ~(_BV(TOIE2));

	sound_is_playing = 0;

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

volatile uint8_t bitpos = 0, nth = 0;
volatile uint8_t sample;

ISR(TIMER2_OVF_vect)
{
	if (sound_is_playing)
	{
		switch (nth)
		{
		case 0:
			if (soundqueue[soundqueueindex].format == SOUND_FORMAT_PCM)
			{
				// Wave form generation on COM2B1
				TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
			}
			else if (soundqueue[soundqueueindex].format == SOUND_FORMAT_BTC)
			{
				TCCR2A = _BV(WGM21) | _BV(WGM20);
			}

			// Load current "speed" - needed for e.g. adjusting the sound pitch of an engine sample
			OCR2A = soundqueue[soundqueueindex].speed;

			++nth;
			break;
		case 2:
			++nth;
			break;
		case 1:
			if (soundqueue[soundqueueindex].format == SOUND_FORMAT_PCM)
			{
				++nth;
				break;
			}
		case 4:
			if ( (++bitpos & 8) || (soundqueue[soundqueueindex].format == SOUND_FORMAT_PCM))
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
					nth = 0;
					// goto TIMER2_OVF_vect_SOUNDSTUFF_DONE;
				}
			}

			++nth;
			break;
		case 3:
		case 5:
			if (soundqueue[soundqueueindex].format == SOUND_FORMAT_BTC)
			{
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

			++nth;
			break;
		case 6:
			if (soundqueue[soundqueueindex].format == SOUND_FORMAT_PCM)
			{
				OCR2B = sample;
			}

			++nth;
			break;
		case  7:
			++nth;
			break;
		default:
			nth = 0;
		}

	}
	else
	{
		nth = 0;
	}

	TIMER2_OVF_vect_SOUNDSTUFF_DONE: ;
}

void soundplayer_setup()
{
	pinMode(3, OUTPUT);

	// setup clock
	ASSR &= ~(_BV(EXCLK) | _BV(AS2));

	TCCR2B = _BV(WGM22) | _BV(CS20);	// No prescaler
}

uint8_t soundplayer_play(uint16_t sounddata_p, uint16_t soundlen, uint8_t format, uint8_t speed,
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
	OCR2A = speed;
	soundqueue[soundqueueindex].format = format;
	soundqueue[soundqueueindex].finishfunc = finishfunc;
	soundqueue[soundqueueindex].finishparam = finishparam;
	soundqueue[soundqueueindex].samplepos = 0;

	TIMSK2 = _BV(TOIE2);
	samplepos = 0;
	sound_is_playing = 1;
	nth = 0;
	sei();

	return soundqueueindex;
}

uint8_t soundplayer_play(uint16_t sounddata_p, uint16_t soundlen, uint8_t format)
{
	return soundplayer_play(sounddata_p, soundlen, format, (uint8_t) MAXCNTRELOAD,
			finishplay_gotoprev, 0);
}

uint8_t soundplayer_play_repeat(uint16_t sounddata_p, uint16_t soundlen, uint8_t format,
		uint8_t repeat)
{
	return soundplayer_play(sounddata_p, soundlen, format, (uint8_t) MAXCNTRELOAD,
			finishplay_repeat, repeat);
}

uint8_t soundplayer_play_ds(uint16_t sounddata_p, uint16_t soundlen, uint8_t format, uint8_t ds)
{
	return soundplayer_play(sounddata_p, soundlen, format, (uint8_t) MAXCNTRELOAD,
			finishplay_durationds, ds);
}

