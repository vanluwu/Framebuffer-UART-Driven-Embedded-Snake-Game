#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int serial_fd = -1;

int panic(void) {
  printf("[Panic] %i: %s\n", errno, strerror(errno));
  return 1;
}

int main(int argc, char *argv[]) {
  struct termios tio;

  if (argc != 3) {
    printf("Usage: %s <led_num 0-4> <on=1, off=0>\n", argv[0]);
    return 1;
  }

  serial_fd = open("/dev/ttymxc1", O_RDWR);
  if (serial_fd == -1)
    return panic();

  bzero(&tio, sizeof(tio));
  tio.c_cflag = B115200 | CRTSCTS | CS8 | CLOCAL;
  tio.c_iflag = IGNPAR;
  tio.c_oflag = 0;
  tio.c_lflag = 0;
  tio.c_cc[VTIME] = 0;
  tio.c_cc[VMIN] = 5;

  if (tcsetattr(serial_fd, TCSANOW, &tio) != 0)
    return panic();

  unsigned char msg[] = {0x12, 0x21, (unsigned char)(0x30 + atoi(argv[1])),
                         (unsigned char)(0x31 - atoi(argv[2])), 0x13};
  write(serial_fd, msg, sizeof(msg));

  printf("LED %s %s\n", argv[1], atoi(argv[2]) ? "ON" : "OFF");
  close(serial_fd);
  return 0;
}
