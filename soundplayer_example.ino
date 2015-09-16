#define DEBUG
#include <Arduino.h>

//#define SAMPLE_RATE 16000

#include "soundplay.h"

// 1-bit sound (see http://www.romanblack.com/picsound.htm

#ifndef SOUNDFORMAT_BTC
// sample tables made with wav2c https://github.com/olleolleolle/wav2c , must be in 8bit mono format
#include "sound_crankup.h"
#include "sound_diesel.h"
#include "sound_hupe.h"
#else // SOUNDFORMAT_BTC
#include "sound_crankup_btc.h"
#include "sound_diesel_btc.h"
#include "sound_horn_btc.h"
#endif // SOUNDFORMAT_BTC

uint8_t backgroundsoundindex, i;
void setup()
{
#ifdef DEBUG
	Serial.begin(115200);
#endif
	pinMode(A1, INPUT);

	soundplayer_setup();

#ifndef SOUNDFORMAT_BTC
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
#else // SOUNDFORMAT_BTC
	backgroundsoundindex = soundplayer_play((uint16_t) &sound_diesel_btc,
				sound_diesel_btc_len, MAXCNTRELOAD, finishplay_repeat, 0);
	delay(3000);
	/*soundplayer_play((uint16_t) &sound_crankup_btc,
			sound_crankup_btc_len);
	delay(3000);
	*/
	#endif // SOUNDFORMAT_BTC
}

void loop()
{
	uint16_t analogVal;

	while (true)
	{
		if (digitalRead (A2))
		{
#ifndef SOUNDFORMAT_BTC
			soundplayer_play_ds((uint16_t) &sound_hupe_data, sound_hupe_length,
					20);
#else // SOUNDFORMAT_BTC
			soundplayer_play_ds((uint16_t) &sound_horn_btc, sound_horn_btc_len,
					20);
#endif // SOUNDFORMAT_BTC
		}
		if (soundqueueindex == backgroundsoundindex)
		{
			analogVal = analogRead(A7);
			soundqueue[backgroundsoundindex].speed = map(analogVal, 0, 1023,
			MAXCNTRELOAD - (255 - 170), MAXCNTRELOAD);
			debugprint(soundqueue[backgroundsoundindex].speed );
		}
	}
}
