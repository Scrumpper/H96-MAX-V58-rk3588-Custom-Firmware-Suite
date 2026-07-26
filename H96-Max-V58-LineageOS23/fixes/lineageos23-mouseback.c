/* androidboox-mouseback — reliable "right-click = Back" for ANY mouse.
 *
 * Why not a key layout? Mapping BTN_RIGHT->BACK in Generic.kl is flaky: Android's
 * cursor mapper still treats BTN_RIGHT as the secondary pointer button, so you get
 * BOTH a right-click AND a BACK key and which wins depends on the app.
 *
 * This daemon instead reads raw evdev from every input device that reports BTN_RIGHT
 * (i.e. any mouse — hardware-agnostic, not device-specific) and injects a clean KEY_BACK
 * through a uinput virtual keyboard. Raw read() has no getevent-style buffering, and the
 * injected key is a normal top-level BACK, so it's reliable. Re-scans for hot-plugged
 * mice every few seconds.
 *
 * SHORT right-click (< LONGPRESS_MS) -> BACK.  LONG-press right-click (>= LONGPRESS_MS)
 * -> no BACK, so the real secondary-button context menu (e.g. the launcher's wallpaper
 * picker) stays open instead of being closed by the injected BACK.
 *
 * Built static (aarch64) so it runs on Android without bionic; runs as a root init
 * service on the androidboox H96 Max V58 build (SELinux permissive).
 */
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define MAXDEV 32
#define LONGPRESS_MS 400            /* >= this held = real right-click (no BACK) */
#define NLONGS(x) (((x) / (8 * sizeof(long))) + 1)

static int test_bit(const unsigned long *a, int bit) {
    return (a[bit / (8 * sizeof(long))] >> (bit % (8 * sizeof(long)))) & 1UL;
}

static int has_btn_right(int fd) {
    unsigned long keys[NLONGS(KEY_MAX)];
    memset(keys, 0, sizeof(keys));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) return 0;
    return test_bit(keys, BTN_RIGHT);
}

static int make_uinput(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) return -1;
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) { close(fd); return -1; }
    ioctl(fd, UI_SET_KEYBIT, KEY_BACK);
    struct uinput_user_dev u;
    memset(&u, 0, sizeof(u));
    snprintf(u.name, sizeof(u.name), "androidboox-mouseback");
    u.id.bustype = BUS_VIRTUAL; u.id.vendor = 1; u.id.product = 1; u.id.version = 1;
    if (write(fd, &u, sizeof(u)) < 0) { close(fd); return -1; }
    if (ioctl(fd, UI_DEV_CREATE) < 0) { close(fd); return -1; }
    return fd;
}

static void emit(int fd, int type, int code, int val) {
    struct input_event e;
    memset(&e, 0, sizeof(e));
    e.type = type; e.code = code; e.value = val;
    if (write(fd, &e, sizeof(e)) < 0) { /* ignore */ }
}

static void send_back(int fd) {
    emit(fd, EV_KEY, KEY_BACK, 1); emit(fd, EV_SYN, SYN_REPORT, 0);
    emit(fd, EV_KEY, KEY_BACK, 0); emit(fd, EV_SYN, SYN_REPORT, 0);
}

static int scan_mice(struct pollfd *p) {
    int n = 0;
    DIR *d = opendir("/dev/input");
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < MAXDEV) {
        if (strncmp(e->d_name, "event", 5) != 0) continue;
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (has_btn_right(fd)) { p[n].fd = fd; p[n].events = POLLIN; n++; }
        else close(fd);
    }
    closedir(d);
    return n;
}

int main(void) {
    int ufd = make_uinput();
    if (ufd < 0) return 1;

    struct pollfd p[MAXDEV];
    int n = scan_mice(p);

    int pressed = 0;                       /* right button currently held? */
    struct timeval pt = {0, 0};            /* when it was pressed (evdev timestamp) */

    for (;;) {
        int r = poll(p, n, 3000);          /* 3s timeout drives periodic re-scan */
        int rescan = (r <= 0);
        for (int i = 0; i < n && r > 0; i++) {
            if (p[i].revents & (POLLERR | POLLHUP)) { rescan = 1; continue; }
            if (!(p[i].revents & POLLIN)) continue;
            struct input_event ev;
            while (read(p[i].fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type != EV_KEY || ev.code != BTN_RIGHT) continue;
                if (ev.value == 1) {                 /* press: start timing */
                    pressed = 1; pt = ev.time;
                } else if (ev.value == 0 && pressed) { /* release: decide */
                    pressed = 0;
                    long ms = (ev.time.tv_sec  - pt.tv_sec)  * 1000L
                            + (ev.time.tv_usec - pt.tv_usec) / 1000L;
                    if (ms >= 0 && ms < LONGPRESS_MS)
                        send_back(ufd);              /* short click -> BACK */
                    /* long press -> leave the real right-click / context menu alone */
                }
            }
        }
        if (rescan) {
            for (int i = 0; i < n; i++) close(p[i].fd);
            n = scan_mice(p);
        }
    }
    return 0;
}
