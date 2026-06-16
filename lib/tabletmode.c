/* fw12tab tablet-mode switch reader.
 *
 * The Framework 12 EC exposes a real SW_TABLET_MODE switch via the INT33D3
 * gpio-keys input device — the same signal GNOME/Windows gate tablet mode on.
 * This finds that device, prints its state (1 = tablet, 0 = laptop) once at
 * startup and again on every change, one line each, flushed.
 *
 * Event-driven, so the watcher needs no polling, has no 500-angle ambiguity,
 * and survives suspend/resume. Pure C, only kernel uapi headers, no deps.
 *
 *   cc -O2 -o tabletmode tabletmode.c
 */
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define NLONGS(x) (((x) / (sizeof(long) * 8)) + 1)
#define test_bit(b, a) (((a)[(b) / (sizeof(long) * 8)] >> ((b) % (sizeof(long) * 8))) & 1UL)

static int has_tablet_sw(int fd) {
  unsigned long ev[NLONGS(EV_MAX)] = {0}, sw[NLONGS(SW_MAX)] = {0};
  if (ioctl(fd, EVIOCGBIT(0, sizeof ev), ev) < 0 || !test_bit(EV_SW, ev)) return 0;
  if (ioctl(fd, EVIOCGBIT(EV_SW, sizeof sw), sw) < 0) return 0;
  return test_bit(SW_TABLET_MODE, sw);
}

static int read_state(int fd) {
  unsigned long st[NLONGS(SW_MAX)] = {0};
  if (ioctl(fd, EVIOCGSW(sizeof st), st) < 0) return 0;
  return test_bit(SW_TABLET_MODE, st) ? 1 : 0;
}

/* Find the first /dev/input/event* that advertises SW_TABLET_MODE. */
static int open_tablet_dev(void) {
  DIR *d = opendir("/dev/input");
  if (!d) return -1;
  struct dirent *e;
  char path[300];
  int fd = -1;
  while ((e = readdir(d))) {
    if (strncmp(e->d_name, "event", 5)) continue;
    snprintf(path, sizeof path, "/dev/input/%s", e->d_name);
    int f = open(path, O_RDONLY);
    if (f < 0) continue;
    if (has_tablet_sw(f)) { fd = f; break; }
    close(f);
  }
  closedir(d);
  return fd;
}

int main(int argc, char **argv) {
  int fd = (argc > 1) ? open(argv[1], O_RDONLY) : open_tablet_dev();
  if (fd < 0) {
    fprintf(stderr, "fw12tab-tabletmode: no SW_TABLET_MODE device (need read on /dev/input/event*)\n");
    return 1;
  }
  printf("%d\n", read_state(fd));
  fflush(stdout);

  struct input_event ev;
  while (read(fd, &ev, sizeof ev) == (ssize_t)sizeof ev) {
    if (ev.type == EV_SW && ev.code == SW_TABLET_MODE) {
      printf("%d\n", ev.value ? 1 : 0);
      fflush(stdout);
    }
  }
  return 0;
}
