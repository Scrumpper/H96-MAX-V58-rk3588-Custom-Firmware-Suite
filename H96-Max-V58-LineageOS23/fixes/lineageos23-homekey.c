/* androidboox-homekey — Windows-key + H (Meta+H) -> HOME, on any keyboard.
 *
 * Meta+H is NOT a default Android/Launcher3 home shortcut, so on the GSI pressing
 * it just fires the (GMS-less, non-functional) assist gesture. This daemon restores
 * "Win+H -> desktop": it watches every keyboard for the Meta modifier being held
 * while H is pressed, and injects a clean KEY_HOME through a uinput virtual keyboard.
 *
 * Passive read() (no EVIOCGRAB), exactly like androidboox-mouseback — it does not
 * consume the keys, so normal typing is untouched; the injected HOME just wins the
 * navigation. Re-scans for hot-plugged keyboards every few seconds. Static aarch64.
 */
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define MAXDEV 32
#define NLONGS(x) (((x) / (8 * sizeof(long))) + 1)

static int test_bit(const unsigned long *a, int bit) {
    return (a[bit / (8 * sizeof(long))] >> (bit % (8 * sizeof(long)))) & 1UL;
}

/* a keyboard = reports the H key (any real keyboard does) */
static int is_keyboard(int fd) {
    unsigned long keys[NLONGS(KEY_MAX)];
    memset(keys, 0, sizeof(keys));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) return 0;
    return test_bit(keys, KEY_H);
}

static int make_uinput(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) return -1;
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) { close(fd); return -1; }
    ioctl(fd, UI_SET_KEYBIT, KEY_HOME);
    struct uinput_user_dev u;
    memset(&u, 0, sizeof(u));
    snprintf(u.name, sizeof(u.name), "androidboox-homekey");
    u.id.bustype = BUS_VIRTUAL; u.id.vendor = 1; u.id.product = 2; u.id.version = 1;
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

static void send_home(int fd) {
    emit(fd, EV_KEY, KEY_HOME, 1); emit(fd, EV_SYN, SYN_REPORT, 0);
    emit(fd, EV_KEY, KEY_HOME, 0); emit(fd, EV_SYN, SYN_REPORT, 0);
}

static int scan_kbds(struct pollfd *p) {
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
        if (is_keyboard(fd)) { p[n].fd = fd; p[n].events = POLLIN; n++; }
        else close(fd);
    }
    closedir(d);
    return n;
}

int main(void) {
    int ufd = make_uinput();
    if (ufd < 0) return 1;

    struct pollfd p[MAXDEV];
    int n = scan_kbds(p);
    int meta_down = 0;                 /* either Windows/Meta key held */

    for (;;) {
        int r = poll(p, n, 3000);
        int rescan = (r <= 0);
        for (int i = 0; i < n && r > 0; i++) {
            if (p[i].revents & (POLLERR | POLLHUP)) { rescan = 1; continue; }
            if (!(p[i].revents & POLLIN)) continue;
            struct input_event ev;
            while (read(p[i].fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type != EV_KEY) continue;
                if (ev.code == KEY_LEFTMETA || ev.code == KEY_RIGHTMETA) {
                    meta_down = (ev.value != 0);
                } else if (ev.code == KEY_H && ev.value == 1 && meta_down) {
                    send_home(ufd);      /* Win+H -> HOME */
                }
            }
        }
        if (rescan) {
            for (int i = 0; i < n; i++) close(p[i].fd);
            n = scan_kbds(p);
        }
    }
    return 0;
}
