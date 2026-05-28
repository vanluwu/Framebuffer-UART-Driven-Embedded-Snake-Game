#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int serial_fd = -1;

int panic(void) {
  printf("[Panic] %i: %s\n", errno, strerror(errno));
  return 1;
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  struct termios tio;

  serial_fd = open("/dev/ttymxc1", O_RDWR);
  if (serial_fd == -1)
    return panic();

  bzero(&tio, sizeof(tio));
  tio.c_cflag = B115200 | CRTSCTS | CS8 | CLOCAL | CREAD;
  tio.c_iflag = IGNPAR;
  tio.c_oflag = 0;
  tio.c_lflag = 0;
  tio.c_cc[VTIME] = 0;
  tio.c_cc[VMIN] = 5;

  if (tcsetattr(serial_fd, TCSANOW, &tio) != 0)
    return panic();

  unsigned char read_buf[5];
  memset(read_buf, '\0', sizeof(read_buf));

  printf("Button monitor started. Press M4 buttons (Ctrl+C to quit).\n\n");

  while (1) {
    int n = read(serial_fd, read_buf, sizeof(read_buf));
    if (n < 0)
      continue;
    if (read_buf[1] != 0x22)
      continue;
    printf("Button [%d] click! State: [%d]\n", read_buf[2] - 0x30,
           read_buf[3] - 0x30);
  }

  close(serial_fd);
  return 0;
}
