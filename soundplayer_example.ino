#define DEBUG
#define ___AVR_ATmega328P__
#include <Arduino.h>

#include "soundplay.h"
// sample tables made with wav2c https://github.com/olleolleolle/wav2c , must be in 8bit mono format
#include "sound_crankup.h"
#include "sound_diesel.h"
#include "sound_hupe.h"

//#define SOUNDFORMAT_BTC // 1-bit sound (see http://www.romanblack.com/picsound.htm)

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
