#define DEBUG
#include <Arduino.h>

//#define SAMPLE_RATE 16000

#include "soundplay.h"

// 1-bit sound (see http://www.romanblack.com/picsound.htm

#include "sounds/sound_crankup.h"
#include "sounds/sound_diesel.h"
#include "sounds/sound_hupe.h"
#include "sounds/sound_feuerwehr_btc.h"

uint8_t backgroundsoundindex, i;
void setup()
{
#ifdef DEBUG
	Serial.begin(115200);
#endif
	pinMode(A1, INPUT);

	soundplayer_setup();

	backgroundsoundindex = soundplayer_play((uint16_t) &sound_diesel,
				sound_diesel_len, SOUND_FORMAT_PCM, MAXCNTRELOAD, finishplay_repeat, 0);
	soundplayer_play_repeat((uint16_t) &sound_crankup,
			sound_crankup_len, SOUND_FORMAT_PCM, 3);
	//delay(3000);
	/*soundplayer_play((uint16_t) &sound_crankup_btc,
			sound_crankup_btc_len);
	delay(3000);
	*/

}

void loop()
{
	uint16_t analogVal;

	while (true)
	{
		if (digitalRead (A2))
		{
			//soundplayer_play_ds((uint16_t) &sound_hupe_data, sound_hupe_length, SOUND_FORMAT_PCM, 20);
			soundplayer_play_ds((uint16_t) &sound_feuerwehr_btc, sound_feuerwehr_btc_len, SOUND_FORMAT_BTC,
					20);
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
