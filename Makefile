TARGET := snake
SRC    := snake.c

ifdef ARM
    CC     := arm-linux-gnueabihf-gcc
    CFLAGS := -static -Wall -O2
else
    CC     := gcc
    CFLAGS := -Wall -O2 -DNO_HARDWARE
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)
	@echo "Build OK: $(TARGET)"
	@file $(TARGET) || true

clean:
	rm -f $(TARGET)

.PHONY: all clean
