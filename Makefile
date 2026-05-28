PROGRAMS := gpio_led bt_event buzzer

ifdef ARM
    CC     := arm-linux-gnueabihf-gcc
    CFLAGS := -static -Wall -O2
else
    CC     := gcc
    CFLAGS := -Wall -O2
endif

all: $(PROGRAMS)

gpio_led: gpio_led.c
	$(CC) $(CFLAGS) -o $@ $<

bt_event: bt_event.c
	$(CC) $(CFLAGS) -o $@ $<

buzzer: buzzer.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGRAMS)

.PHONY: all clean
