#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CELL_SIZE 24
#define GRID_W 40
#define GRID_H 20
#define MAX_SNAKE (GRID_W * GRID_H)
#define INITIAL_DELAY 200000
#define MIN_DELAY 60000
#define LEVEL_STEP 50

// Colors (32-bit BGRA: B G R A in memory order)
#define COL_BG 0x00102030
#define COL_GRID 0x00203040
#define COL_BORDER 0x00FFFFFF
#define COL_SNAKE_BODY 0x0000C800
#define COL_SNAKE_HEAD 0x0000FF80
#define COL_FOOD 0x00FF2020
#define COL_TEXT 0x00FFFFFF
#define COL_PAUSE 0x002080FF
#define COL_OVER 0x00FF0000

// Q6 hardware paths
#define SYSFS_GPIO "/sys/class/gpio"
#define EVENT_DEV "/dev/input/event1"
#define BUZZER_DEV "/dev/buzzer"
#define M4_DEV "/dev/ttymxc1"

#define GPIO(B, IO) ((B - 1) * 32 + (IO))
#define LED_BACK GPIO(1, 21)
#define LED_HOME GPIO(1, 16)
#define LED_MENU GPIO(1, 20)

// Buttons
#define KEY_BTN_BACK 158
#define KEY_BTN_HOME 172
#define KEY_BTN_MENU 139
#define KEY_BTN_VOL_UP 115
#define KEY_BTN_VOL_DOWN 114

// Buzzer
#define IOCTL_START_BUZZER _IOW('b', 0x07, int)
#define IOCTL_END_BUZZER _IOW('b', 0x09, int)
#define IOCTL_SET_TONE _IOW('b', 0x0b, int)
#define IOCTL_SET_VOLUME _IOW('b', 0x0c, int)
#define BUZZER_VOL 20000

// M4 protocol
#define M4_STX 0x12
#define M4_ETX 0x13
#define M4_CMD_LED 0x21
#define M4_CMD_FND 0x23
#define M4_CMD_LCD 0x24

// Direction
#define DIR_RIGHT 0
#define DIR_LEFT 1
#define DIR_UP 2
#define DIR_DOWN 3

// Globals
typedef struct {
  int x, y;
} Point;
typedef struct {
  Point body[MAX_SNAKE];
  int length;
  int direction;
} Snake;

static Snake snake;
static Point food;
static int score, level, high_score;
static int game_over, paused;

// Framebuffer
static int fb_fd = -1;
static uint32_t *fb_ptr = NULL;
static int fb_w = 0, fb_h = 0;
static size_t fb_bytes = 0;

// Hardware fds
static int event_fd = -1, buzzer_fd = -1, m4_fd = -1;

// Game area (centered on screen)
static int game_x = 0, game_y = 0;
static int game_pix_w = 0, game_pix_h = 0;

// Framebuffer
int fb_init(void) {
  fb_fd = open("/dev/fb0", O_RDWR);
  if (fb_fd < 0) {
    perror("open /dev/fb0");
    return -1;
  }

  struct fb_var_screeninfo vinfo;
  if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
    perror("FBIOGET_VSCREENINFO");
    return -1;
  }
  fb_w = vinfo.xres;
  fb_h = vinfo.yres;
  fb_bytes = (size_t)fb_w * fb_h * 4;

  fb_ptr = mmap(NULL, fb_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
  if (fb_ptr == MAP_FAILED) {
    perror("mmap");
    return -1;
  }

  // Center game area on screen
  game_pix_w = GRID_W * CELL_SIZE;
  game_pix_h = GRID_H * CELL_SIZE;
  game_x = (fb_w - game_pix_w) / 2;
  game_y = (fb_h - game_pix_h) / 2;

  return 0;
}

void fb_close(void) {
  if (fb_ptr && fb_ptr != MAP_FAILED)
    munmap(fb_ptr, fb_bytes);
  if (fb_fd >= 0)
    close(fb_fd);
  fb_ptr = NULL;
  fb_fd = -1;
}

void fb_clear(uint32_t color) {
  if (!fb_ptr)
    return;
  for (int i = 0; i < fb_w * fb_h; i++)
    fb_ptr[i] = color;
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
  if (!fb_ptr)
    return;
  int x0 = x < 0 ? 0 : x;
  int y0 = y < 0 ? 0 : y;
  int x1 = (x + w > fb_w) ? fb_w : (x + w);
  int y1 = (y + h > fb_h) ? fb_h : (y + h);
  for (int py = y0; py < y1; py++) {
    uint32_t *row = fb_ptr + py * fb_w;
    for (int px = x0; px < x1; px++)
      row[px] = color;
  }
}

void fb_stroke_rect(int x, int y, int w, int h, int thick, uint32_t color) {
  fb_fill_rect(x, y, w, thick, color);             /* top */
  fb_fill_rect(x, y + h - thick, w, thick, color); /* bot */
  fb_fill_rect(x, y, thick, h, color);             /* left */
  fb_fill_rect(x + w - thick, y, thick, h, color); /* right */
}

// Draw an arrow inside a cell to show direction
void fb_draw_arrow(int cx, int cy, int dir, uint32_t color) {
  int s = CELL_SIZE;
  int margin = 4;
  switch (dir) {
  case DIR_RIGHT: {
    // triangle pointing right
    for (int i = 0; i < s - 2 * margin; i++) {
      int w = (s - 2 * margin) - i;
      fb_fill_rect(cx + margin + i, cy + margin + i / 2, 1, w, color);
    }
  } break;
  case DIR_LEFT: {
    for (int i = 0; i < s - 2 * margin; i++) {
      int w = i + 1;
      fb_fill_rect(cx + margin + i, cy + margin + (s - 2 * margin - w) / 2, 1,
                   w, color);
    }
  } break;
  case DIR_UP: {
    for (int i = 0; i < s - 2 * margin; i++) {
      int w = i + 1;
      fb_fill_rect(cx + margin + (s - 2 * margin - w) / 2, cy + margin + i, w,
                   1, color);
    }
  } break;
  case DIR_DOWN: {
    for (int i = 0; i < s - 2 * margin; i++) {
      int w = (s - 2 * margin) - i;
      fb_fill_rect(cx + margin + i / 2, cy + margin + i, w, 1, color);
    }
  } break;
  }
}

// Q6 LED (sysfs)
static void gpio_write(const char *path, const char *val) {
  int fd = open(path, O_WRONLY);
  if (fd < 0)
    return;
  write(fd, val, strlen(val));
  close(fd);
}

void led_export(int gpio) {
  char path[64], num[16];
  snprintf(num, sizeof(num), "%d", gpio);
  gpio_write(SYSFS_GPIO "/export", num);
  usleep(50000);
  snprintf(path, sizeof(path), SYSFS_GPIO "/gpio%d/direction", gpio);
  gpio_write(path, "out");
}

void led_set(int gpio, int on) {
  char path[64];
  snprintf(path, sizeof(path), SYSFS_GPIO "/gpio%d/value", gpio);
  gpio_write(path, on ? "1" : "0");
}

void led_init_all(void) {
  led_export(LED_BACK);
  led_export(LED_HOME);
  led_export(LED_MENU);
  led_set(LED_BACK, 0);
  led_set(LED_HOME, 0);
  led_set(LED_MENU, 0);
}

void led_cleanup(void) {
  led_set(LED_BACK, 0);
  led_set(LED_HOME, 0);
  led_set(LED_MENU, 0);
}

// Buzzer
void buzzer_init(void) { buzzer_fd = open(BUZZER_DEV, O_RDONLY); }

void buzzer_close(void) {
  if (buzzer_fd >= 0) {
    close(buzzer_fd);
    buzzer_fd = -1;
  }
}

void play_tone(double freq, int ms) {
  if (buzzer_fd < 0)
    return;
  long tone = (long)((1.0 / freq) * 1000000000.0);
  ioctl(buzzer_fd, IOCTL_SET_VOLUME, BUZZER_VOL);
  ioctl(buzzer_fd, IOCTL_SET_TONE, tone);
  ioctl(buzzer_fd, IOCTL_START_BUZZER, 0);
  usleep(ms * 1000);
  ioctl(buzzer_fd, IOCTL_END_BUZZER, 0);
}

void sound_eat(void) { play_tone(523, 80); }
void sound_levelup(void) {
  play_tone(523, 80);
  play_tone(659, 80);
  play_tone(784, 120);
}
void sound_die(void) {
  play_tone(440, 150);
  play_tone(330, 150);
  play_tone(220, 400);
}

// M4 communication
void m4_init(void) {
  m4_fd = open(M4_DEV, O_RDWR);
  if (m4_fd < 0)
    return;
  struct termios tio;
  bzero(&tio, sizeof(tio));
  tio.c_cflag = B115200 | CRTSCTS | CS8 | CLOCAL | CREAD;
  tio.c_iflag = IGNPAR;
  tio.c_cc[VTIME] = 0;
  tio.c_cc[VMIN] = 5;
  tcsetattr(m4_fd, TCSANOW, &tio);
}

void m4_close(void) {
  if (m4_fd >= 0) {
    close(m4_fd);
    m4_fd = -1;
  }
}

void m4_send(unsigned char cmd, unsigned char v1, unsigned char v2) {
  if (m4_fd < 0)
    return;
  unsigned char msg[5] = {M4_STX, cmd, v1, v2, M4_ETX};
  write(m4_fd, msg, 5);
}

void m4_led(int n, int on) { m4_send(M4_CMD_LED, 0x30 + n, 0x31 - on); }
void m4_fnd(int n, int d) { m4_send(M4_CMD_FND, 0x30 + n, 0x30 + d); }
void m4_lcd(int color) { m4_send(M4_CMD_LCD, 0x30, 0x30 + color); }

void m4_show_score(int s) {
  if (s < 0)
    s = 0;
  if (s > 999)
    s = 999;
  m4_fnd(0, (s / 100) % 10);
  m4_fnd(1, (s / 10) % 10);
  m4_fnd(2, s % 10);
}

void m4_state(int st) {
  // 0=idle, 1=play, 2=pause, 3=over
  m4_led(0, 1);
  m4_led(1, st == 1);
  m4_led(2, st == 2);
  m4_led(3, st == 3);
  switch (st) {
  case 0:
    m4_lcd(3);
    break; // Logo
  case 1:
    m4_lcd(1);
    break; // Green
  case 2:
    m4_lcd(2);
    break; // Blue
  case 3:
    m4_lcd(0);
    break; // Red
  }
}

// Button input (non-blocking)
void button_init(void) { event_fd = open(EVENT_DEV, O_RDONLY | O_NONBLOCK); }

void button_close(void) {
  if (event_fd >= 0) {
    close(event_fd);
    event_fd = -1;
  }
}

int button_poll(void) {
  if (event_fd < 0)
    return -1;
  struct input_event ev;
  if (read(event_fd, &ev, sizeof(ev)) != sizeof(ev))
    return -1;
  if (ev.type == 1 && ev.value == 1)
    return ev.code;
  return -1;
}

// Game logic
void place_food(void) {
  for (int t = 0; t < 1000; t++) {
    food.x = 1 + rand() % (GRID_W - 2);
    food.y = 1 + rand() % (GRID_H - 2);
    int hit = 0;
    for (int i = 0; i < snake.length; i++)
      if (snake.body[i].x == food.x && snake.body[i].y == food.y) {
        hit = 1;
        break;
      }
    if (!hit)
      return;
  }
}

void init_game(void) {
  snake.length = 3;
  snake.direction = DIR_RIGHT;
  snake.body[0].x = GRID_W / 2;
  snake.body[0].y = GRID_H / 2;
  snake.body[1].x = GRID_W / 2 - 1;
  snake.body[1].y = GRID_H / 2;
  snake.body[2].x = GRID_W / 2 - 2;
  snake.body[2].y = GRID_H / 2;
  score = 0;
  level = 1;
  game_over = 0;
  paused = 0;
  srand((unsigned)time(NULL));
  place_food();
  led_set(LED_BACK, 1);
  led_set(LED_HOME, 0);
  led_set(LED_MENU, 0);
  m4_state(1);
  m4_show_score(0);
}

void set_dir(int d) {
  if (d == DIR_UP && snake.direction == DIR_DOWN)
    return;
  if (d == DIR_DOWN && snake.direction == DIR_UP)
    return;
  if (d == DIR_LEFT && snake.direction == DIR_RIGHT)
    return;
  if (d == DIR_RIGHT && snake.direction == DIR_LEFT)
    return;
  snake.direction = d;
}

void toggle_pause(void) {
  paused = !paused;
  if (paused) {
    led_set(LED_BACK, 0);
    led_set(LED_HOME, 1);
    m4_state(2);
  } else {
    led_set(LED_BACK, 1);
    led_set(LED_HOME, 0);
    m4_state(1);
  }
}

void move_snake(void) {
  for (int i = snake.length - 1; i > 0; i--)
    snake.body[i] = snake.body[i - 1];
  switch (snake.direction) {
  case DIR_RIGHT:
    snake.body[0].x++;
    break;
  case DIR_LEFT:
    snake.body[0].x--;
    break;
  case DIR_UP:
    snake.body[0].y--;
    break;
  case DIR_DOWN:
    snake.body[0].y++;
    break;
  }
}

int check_collision(void) {
  if (snake.body[0].x <= 0 || snake.body[0].x >= GRID_W - 1 ||
      snake.body[0].y <= 0 || snake.body[0].y >= GRID_H - 1)
    return 1;
  for (int i = 1; i < snake.length; i++)
    if (snake.body[0].x == snake.body[i].x &&
        snake.body[0].y == snake.body[i].y)
      return 1;
  return 0;
}

void check_food(void) {
  if (snake.body[0].x == food.x && snake.body[0].y == food.y) {
    if (snake.length < MAX_SNAKE - 1)
      snake.length++;
    score += 10;
    sound_eat();
    m4_show_score(score);
    if (score % LEVEL_STEP == 0) {
      level++;
      sound_levelup();
    }
    place_food();
  }
}

// Rendering
void draw_cell(int gx, int gy, uint32_t color) {
  int x = game_x + gx * CELL_SIZE;
  int y = game_y + gy * CELL_SIZE;
  fb_fill_rect(x + 1, y + 1, CELL_SIZE - 2, CELL_SIZE - 2, color);
}

void draw_head(int gx, int gy, int dir) {
  int x = game_x + gx * CELL_SIZE;
  int y = game_y + gy * CELL_SIZE;
  fb_fill_rect(x + 1, y + 1, CELL_SIZE - 2, CELL_SIZE - 2, COL_SNAKE_HEAD);
  fb_draw_arrow(x, y, dir, COL_BG);
}

void render(void) {
  // Background
  fb_clear(COL_BG);

  // Border
  fb_stroke_rect(game_x - 4, game_y - 4, game_pix_w + 8, game_pix_h + 8, 4,
                 COL_BORDER);

  // Walls (top/bottom/left/right rows of the grid)
  for (int x = 0; x < GRID_W; x++) {
    draw_cell(x, 0, COL_BORDER);
    draw_cell(x, GRID_H - 1, COL_BORDER);
  }
  for (int y = 0; y < GRID_H; y++) {
    draw_cell(0, y, COL_BORDER);
    draw_cell(GRID_W - 1, y, COL_BORDER);
  }

  // Food
  draw_cell(food.x, food.y, COL_FOOD);

  // Snake body
  for (int i = 1; i < snake.length; i++) {
    draw_cell(snake.body[i].x, snake.body[i].y, COL_SNAKE_BODY);
  }
  // Snake head
  draw_head(snake.body[0].x, snake.body[0].y, snake.direction);

  // Pause indicator: paint border in orange
  if (paused) {
    fb_stroke_rect(game_x - 4, game_y - 4, game_pix_w + 8, game_pix_h + 8, 4,
                   COL_PAUSE);
  }
}

void render_title(void) {
  fb_clear(COL_BG);
  // Center large block as visual cue
  int bw = 600, bh = 100;
  int bx = (fb_w - bw) / 2;
  int by = (fb_h - bh) / 2 - 80;
  fb_stroke_rect(bx, by, bw, bh, 4, COL_BORDER);
  fb_fill_rect(bx + 8, by + 8, bw - 16, bh - 16, COL_SNAKE_BODY);

  //"Press button" indicator
  int sw = 400, sh = 40;
  int sx = (fb_w - sw) / 2;
  int sy = (fb_h - sh) / 2 + 80;
  fb_fill_rect(sx, sy, sw, sh, COL_SNAKE_HEAD);
}

void render_gameover(void) {
  // Overlay red border
  fb_stroke_rect(game_x - 4, game_y - 4, game_pix_w + 8, game_pix_h + 8, 8,
                 COL_OVER);
}

// Cleanup
void cleanup(void) {
  led_cleanup();
  button_close();
  buzzer_close();
  for (int i = 0; i < 5; i++)
    m4_led(i, 0);
  m4_lcd(3);
  m4_close();
  if (fb_ptr) {
    fb_clear(0);
  }
  fb_close();
}

void on_signal(int s) {
  (void)s;
  cleanup();
  exit(0);
}

// Main
int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("Snake Framebuffer — initializing...\n");

  if (fb_init() < 0) {
    fprintf(stderr, "Framebuffer init failed.\n");
    return 1;
  }
  printf("Framebuffer: %dx%d, game area at (%d,%d) size %dx%d\n", fb_w, fb_h,
         game_x, game_y, game_pix_w, game_pix_h);

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  led_init_all();
  button_init();
  buzzer_init();
  m4_init();
  m4_state(0);
  m4_show_score(0);

  printf("Press any button on board to start. VOL- to quit.\n");

restart:
  // Reset M4 to title/idle state
  m4_state(0);
  m4_show_score(0);

  render_title();
  // Wait for any button (debounce: ignore multiple)
  while (1) {
    int k = button_poll();
    if (k > 0 && k != KEY_BTN_VOL_DOWN)
      break;
    if (k == KEY_BTN_VOL_DOWN) {
      cleanup();
      return 0;
    }
    usleep(50000);
  }

  init_game();
  int delay_us = INITIAL_DELAY;

  while (!game_over) {
    // Drain button events
    int k;
    while ((k = button_poll()) > 0) {
      switch (k) {
      case KEY_BTN_BACK:
        set_dir(DIR_UP);
        break;
      case KEY_BTN_HOME:
        set_dir(DIR_DOWN);
        break;
      case KEY_BTN_MENU:
        set_dir(DIR_LEFT);
        break;
      case KEY_BTN_VOL_UP:
        set_dir(DIR_RIGHT);
        break;
      case KEY_BTN_VOL_DOWN:
        toggle_pause();
        break;
      }
    }

    if (!paused) {
      move_snake();
      if (check_collision()) {
        game_over = 1;
        break;
      }
      check_food();
      delay_us = INITIAL_DELAY - (level - 1) * 20000;
      if (delay_us < MIN_DELAY)
        delay_us = MIN_DELAY;
    }

    render();
    usleep(delay_us);
  }

  // Game over
  led_set(LED_BACK, 0);
  led_set(LED_HOME, 0);
  led_set(LED_MENU, 1);
  m4_state(3);
  m4_show_score(score);
  sound_die();

  render();
  render_gameover();

  // Wait for VOL+ (restart) or VOL- (quit)
  while (1) {
    int k = button_poll();
    if (k == KEY_BTN_VOL_UP || k == KEY_BTN_BACK || k == KEY_BTN_HOME ||
        k == KEY_BTN_MENU) {
      if (score > high_score)
        high_score = score;
      goto restart;
    }
    if (k == KEY_BTN_VOL_DOWN)
      break;
    usleep(50000);
  }

  cleanup();
  printf("Goodbye.\n");
  return 0;
}
