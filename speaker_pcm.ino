/*
 * speaker_pcm
 *
 * Plays 8-bit PCM audio on pin 11 using pulse-width modulation (PWM).
 * For Arduino with Atmega168 at 16 MHz.
 *
 * Uses two timers. The first changes the sample value 8000 times a second.
 * The second holds pin 11 high for 0-255 ticks out of a 256-tick cycle,
 * depending on sample value. The second timer repeats 62500 times per second
 * (16000000 / 256), much faster than the playback rate (8000 Hz), so
 * it almost sounds halfway decent, just really quiet on a PC speaker.
 *
 * Takes over Timer 1 (16-bit) for the 8000 Hz timer. This breaks PWM
 * (analogWrite()) for Arduino pins 9 and 10. Takes Timer 2 (8-bit)
 * for the pulse width modulation, breaking PWM for pins 11 & 3.
 *
 * References:
 *     http://www.uchobby.com/index.php/2007/11/11/arduino-sound-part-1/
 *     http://www.atmel.com/dyn/resources/prod_documents/doc2542.pdf
 *     http://www.evilmadscientist.com/article.php/avrdac
 *     http://gonium.net/md/2006/12/27/i-will-think-before-i-code/
 *     http://fly.cc.fer.hr/GDM/articles/sndmus/speaker2.html
 *     http://www.gamedev.net/reference/articles/article442.asp
 *
 * Michael Smith <michael@hurts.ca>
 */
#include "Arduino.h"
#include <stdint.h>
#define ___AVR_ATmega328P__
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

// if sample reate is not 7843Hz, then adjust it here:
// #define SAMPLE_RATE 8000

// if interrupt load is too high, comment out to go without oversampling
// #define NO_OVERSAMPLING

/*
 * The audio data needs to be unsigned, 8-bit and small enough
 * to fit in flash. 10000-13000 samples is about the limit.
 *
 * sounddata.h should look like this:
 *     const int sounddata_length=10000;
 *     const unsigned char sounddata_data[] PROGMEM = { ..... };
 *
 * You can use wav2c from GBA CSS:
 *     http://thieumsweb.free.fr/english/gbacss.html
 * Then add "PROGMEM" in the right place. I hacked it up to dump the samples
 * as unsigned rather than signed, but it shouldn't matter.
 *
 * http://musicthing.blogspot.com/2005/05/tiny-music-makers-pt-4-mac-startup.html
 * mplayer -ao pcm macstartup.mp3
 * sox audiodump.wav -v 1.32 -c 1 -r 8000 -u -1 macstartup-8000.wav
 * sox macstartup-8000.wav macstartup-cut.wav trim 0 10000s
 * wav2c macstartup-cut.wav sounddata.h sounddata
 *
 * (starfox) nb. under sox 12.18 (distributed in CentOS 5), i needed to run
 * the following command to convert my wav file to the appropriate format:
 * sox audiodump.wav -c 1 -r 8000 -u -b macstartup-8000.wav
 */

#ifdef SAMPLE_RATE
#define MAXCNTRELOAD F_CPU / 255 / 8 / SAMPLE_RATE
#else
#define MAXCNTRELOAD 255
#endif


#include "sounddata.h"
#include "sound_diesel.h"
#include "sound_hupe.h"

volatile uint16_t sample = 0;
uint16_t analogVal;

struct soundqueue_item_t {
	uint16_t sounddata_p;
	uint16_t soundlen;
	uint16_t sample;
	uint8_t speed;
	void (*finishfunc)(uint8_t);
	uint8_t finishparam;
};

#define SOUNDQUEUEDEPTH 2
struct soundqueue_item_t soundqueue[SOUNDQUEUEDEPTH];
uint8_t soundqueuecount = 0, soundqueueindex = 0;

void stopPlayback()
{
    // Disable interrupt.
	TIMSK2  &= ~(_BV(TOIE2));
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
		stopPlayback();
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
		// return;
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
uint8_t repeat = 1;

//void (*finishplayfunc)(uint8_t);

ISR(TIMER2_OVF_vect){
#ifndef NO_OVERSAMPLING
	if (nth > 0)
	{
		--nth;
		return;
	}
	nth = 7;
#endif // NO_OVERSAMPLING

    if (sample >= soundqueue[soundqueueindex].soundlen) {
    	sample = 0;
    	soundqueue[soundqueueindex].finishfunc(soundqueue[soundqueueindex].finishparam);
    }

    OCR2B = pgm_read_byte(soundqueue[soundqueueindex].sounddata_p + sample);
    ++sample;
}

void setupPlayback()
{
    pinMode(3, OUTPUT);

    // Set up Timer 2 to do pulse width modulation on the speaker
    // pin.

    // Use internal clock (datasheet p.160)
    ASSR &= ~(_BV(EXCLK) | _BV(AS2));

	TCCR2A =  _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);

#ifndef NO_OVERSAMPLING
	TCCR2B =  _BV(WGM22)| _BV(CS20);	// No prescaler
#else
	TCCR2B =  _BV(WGM22)| _BV(CS21);	// /8 prescaler
#endif // NO_OVERSAMPLING

	OCR2A = MAXCNTRELOAD;
}

uint8_t startPlayback(uint16_t sounddata_p, uint16_t soundlen, uint8_t speed, void (*finishfunc)(uint8_t), uint8_t finishparam)
{
	uint8_t soundqueueoldindex;

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
    	if (soundqueueindex > 1 )
    	{
			// save current speed
			soundqueue[soundqueueindex - 1].speed = OCR2A;
			// save current position
			soundqueue[soundqueueindex - 1].sample = sample;
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
    sample = 0;
    sei();

    return soundqueueindex;
}

void finishplay_repeat()
{
	sample = 0;
}


void setup()
{
	setupPlayback();
    startPlayback((uint16_t) &sound_diesel_data, sound_diesel_length, MAXCNTRELOAD, finishplay_repeat, 0);
    delay(2000);
    startPlayback((uint16_t) &sound_hupe_data, sound_hupe_length, MAXCNTRELOAD, finishplay_durationds, 30);
    // delay(3000);
    //startPlayback((uint16_t) &sounddata_data, sounddata_length, MAXCNTRELOAD, finishplay_hupe);
    //delay(1000);
}

void loop()
{

    while (true)
    {
    	analogVal = analogRead(A7);
    	OCR2A = map(analogVal, 0, 1023, 170, MAXCNTRELOAD);
    }
}
