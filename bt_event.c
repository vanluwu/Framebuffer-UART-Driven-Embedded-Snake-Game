

#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  int fd;
  if (argc < 2) {
    printf("./bt_event <device>\n");
    printf("Example: ./bt_event /dev/input/event1\n");
    return 1;
  }

  fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    printf("Cannot open %s\n", argv[1]);
    return 1;
  }

  struct input_event ev;
  printf("Application Started!\n");
  printf("Press buttons on board, Ctrl+C to quit.\n\n");

  while (1) {
    read(fd, &ev, sizeof(struct input_event));
    if (ev.type == 1) {
      printf("Key Code [%i] = State is %i\n", ev.code, ev.value);
    }
  }

  close(fd);
  return 0;
}
