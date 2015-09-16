#BOARD:=mega2560
BOARD:=nano328
BOARD_TAG:=nano328
#pro5v328
MONITOR_PORT:=/dev/ttyUSB0

# from: http://ed.am/dev/make/arduino-mk
include /usr/share/arduino/Arduino.mk

BTCFILES:=sound_*.btc

btc: %_btc.h

%_btc.h: $(BTCFILES)
	for BTCFILE in $^ ; do \
		SOUNDHFILE=`echo -n $${BTCFILE} | sed -e 's/^sound_//i;s/\.btc/_btc/i;s/^/sound_/;'`; \
		xxd -i $$BTCFILE | sed -e 's/unsigned /const unsigned /g;s/\[\] = {/[] PROGMEM = {/g' > $${SOUNDHFILE}.h ; \
	done

