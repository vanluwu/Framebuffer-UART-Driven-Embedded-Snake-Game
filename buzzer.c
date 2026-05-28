
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define IOCTL_START_BUZZER _IOW('b', 0x07, int)
#define IOCTL_END_BUZZER _IOW('b', 0x09, int)
#define IOCTL_SET_TONE _IOW('b', 0x0b, int)
#define IOCTL_SET_VOLUME _IOW('b', 0x0c, int)
#define IOCTL_GET_TONE _IOW('b', 0x0d, int)
#define IOCTL_GET_VOLUME _IOW('b', 0x0e, int)

long freqToTone(double freq) { return (long)((1.0f / freq) * 1000000000); }

void playTone(long tone, int volume, int time_us) {
  int buzzer_fd = open("/dev/buzzer", O_RDONLY);
  if (buzzer_fd < 0) {
    perror("open /dev/buzzer");
    return;
  }
  ioctl(buzzer_fd, IOCTL_SET_VOLUME, volume);
  ioctl(buzzer_fd, IOCTL_SET_TONE, tone);
  printf("Tone: %lu, Volume: %d\n", tone, volume);

  ioctl(buzzer_fd, IOCTL_START_BUZZER, 0);
  usleep(time_us);
  ioctl(buzzer_fd, IOCTL_END_BUZZER, 0);
  close(buzzer_fd);
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  double TONEHZ[8] = {523, 587, 659, 698, 783, 880, 987, 1046};

  printf("Playing 8 notes...\n");
  for (int i = 0; i < 8; i++) {
    long tone = freqToTone(TONEHZ[i]);
    playTone(tone, 25000, 100 * 1000);
  }
  printf("Done.\n");
  return 0;
}
