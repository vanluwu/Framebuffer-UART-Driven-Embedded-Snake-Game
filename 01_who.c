
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

  unsigned char msg[] = {0x12, 0x25, 0x00, 0x00, 0x13};
  write(serial_fd, msg, sizeof(msg));

  unsigned char read_buf[5];
  memset(read_buf, '\0', sizeof(read_buf));
  int n = read(serial_fd, read_buf, sizeof(read_buf));
  if (n < 0)
    return panic();

  if (read_buf[2] == 0xFF && read_buf[3] == 0xFF) {
    printf("M4 is ready to go!\n");
  } else {
    printf("M4 response unexpected: %02X %02X %02X %02X %02X\n", read_buf[0],
           read_buf[1], read_buf[2], read_buf[3], read_buf[4]);
  }

  close(serial_fd);
  return 0;
}
