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

  if (argc != 2) {
    printf("Usage: %s <color 0=Red, 1=Green, 2=Blue, 3=Logo>\n", argv[0]);
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

  unsigned char msg[] = {0x12, 0x24, 0x30,
                         (unsigned char)(0x30 + atoi(argv[1])), 0x13};
  write(serial_fd, msg, sizeof(msg));

  const char *names[] = {"Red", "Green", "Blue", "Logo"};
  int c = atoi(argv[1]);
  printf("LCD = %s\n", (c >= 0 && c < 4) ? names[c] : "?");
  close(serial_fd);
  return 0;
}
