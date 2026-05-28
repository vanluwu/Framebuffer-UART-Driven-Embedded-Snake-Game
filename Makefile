PROGRAMS := who led fnd lcd button

ifdef ARM
    CC     := arm-linux-gnueabihf-gcc
    CFLAGS := -static -Wall -O2
else
    CC     := gcc
    CFLAGS := -Wall -O2
endif

all: $(PROGRAMS)

who:    01_who.c     ; $(CC) $(CFLAGS) -o $@ $<
led:    02_led.c     ; $(CC) $(CFLAGS) -o $@ $<
fnd:    03_fnd.c     ; $(CC) $(CFLAGS) -o $@ $<
lcd:    04_lcd.c     ; $(CC) $(CFLAGS) -o $@ $<
button: 05_button.c  ; $(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGRAMS)

.PHONY: all clean
