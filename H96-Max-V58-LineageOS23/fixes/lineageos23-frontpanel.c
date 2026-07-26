// androidboox-frontpanel: front-panel VFD driver for the H96 Max V58.
//
// Replaces the stock 'datadis' clock daemon. Writes the 5-byte segment buffer
// to /dev/digital_dis (FD650/TM1650-class VFD):
//   byte 0..3 = the four 7-seg clock digits (HHMM, 24h)
//   byte 4    = indicator grid. Bit map (decoded on real hardware):
//       0x01 ethernet/LAN   0x02 WIFI        0x04 pause      0x08 play+alarm
//       0x10 colon          0x20 unused      0x40 USB        0x80 unused
//
// The panel does NOT latch, so we refresh continuously from a single kept-open
// fd (the open/close churn of reopening per write reboots the driver).
//
// Icons reflect live system state:
//   ethernet -> eth0 link up      WIFI -> wlan0 link up
//   USB      -> a USB block dev (sd*) present
//   play     -> any ALSA playback PCM RUNNING  (also lights alarm: shared segment)

#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/wait.h>

static const unsigned char FONT[10] =
    {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};

static int read_file(const char *path, char *buf, int len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, buf, len - 1);
    close(fd);
    if (n < 0) n = 0;
    buf[n] = 0;
    return n;
}

// interface link is "up" (associated + carrier) per operstate
static int link_up(const char *iface) {
    char p[128], b[16];
    snprintf(p, sizeof(p), "/sys/class/net/%s/operstate", iface);
    if (read_file(p, b, sizeof(b)) <= 0) return 0;
    return strncmp(b, "up", 2) == 0;
}

// any USB mass-storage block device present (/sys/block/sd*)
static int usb_present(void) {
    DIR *d = opendir("/sys/block");
    if (!d) return 0;
    struct dirent *e; int found = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == 's' && e->d_name[1] == 'd') { found = 1; break; }
    }
    closedir(d);
    return found;
}

// any ALSA playback substream actively RUNNING
static int audio_playing(void) {
    DIR *d = opendir("/proc/asound");
    if (!d) return 0;
    struct dirent *e; int running = 0;
    char path[256], buf[64];
    while ((e = readdir(d)) && !running) {
        if (strncmp(e->d_name, "card", 4) != 0) continue;
        for (int p = 0; p < 4 && !running; p++) {
            for (int s = 0; s < 2 && !running; s++) {
                snprintf(path, sizeof(path),
                         "/proc/asound/%s/pcm%dp/sub%d/status", e->d_name, p, s);
                if (read_file(path, buf, sizeof(buf)) > 0 && strstr(buf, "RUNNING"))
                    running = 1;
            }
        }
    }
    closedir(d);
    return running;
}

// Read local HH:MM from Android's `date` (which honours persist.sys.timezone).
// This daemon is a static glibc binary, so its own localtime() would ignore the
// Android timezone and show UTC — hence we shell out to /system/bin/date instead.
// fork/exec directly (glibc popen wants /bin/sh, which Android lacks).
static void read_local_hhmm(int *hh, int *mm) {
    int pf[2];
    if (pipe(pf) != 0) return;
    pid_t p = fork();
    if (p == 0) {
        dup2(pf[1], 1); close(pf[0]); close(pf[1]);
        execl("/system/bin/date", "date", "+%H%M", (char *)0);
        _exit(127);
    }
    close(pf[1]);
    char b[16] = {0};
    if (read(pf[0], b, sizeof(b) - 1) < 0) b[0] = 0;
    close(pf[0]);
    waitpid(p, 0, 0);
    int v = atoi(b);
    if (v >= 0 && v <= 2359) { *hh = v / 100; *mm = v % 100; }
}

int main(void) {
    int fd = open("/dev/digital_dis", O_WRONLY);
    if (fd < 0) { perror("open /dev/digital_dis"); return 1; }

    unsigned int tick = 0;
    int colon = 0, hh = 0, mm = 0;
    unsigned char base = 0;   // icon bits WITHOUT the colon

    for (;;) {
        // refresh live state ~once/second (every 8 ticks of 125ms)
        if (tick % 8 == 0) {
            base = 0;
            if (link_up("eth0"))  base |= 0x01;  // ethernet
            if (link_up("wlan0")) base |= 0x02;  // WIFI
            if (usb_present())    base |= 0x40;  // USB
            if (audio_playing())  base |= 0x08;  // play (+ alarm, shared)
        }
        // refresh the clock from Android local time ~every 2s (every 16 ticks)
        if (tick % 16 == 0) read_local_hhmm(&hh, &mm);
        // blink the colon ~every 500ms (every 4 ticks)
        if (tick % 4 == 0) colon = !colon;

        unsigned char buf[5];
        buf[0] = FONT[(hh / 10) % 10];
        buf[1] = FONT[hh % 10];
        buf[2] = FONT[(mm / 10) % 10];
        buf[3] = FONT[mm % 10];
        buf[4] = base | (colon ? 0x10 : 0);

        if (write(fd, buf, 5) < 0) {
            // driver hiccup: reopen once and keep going
            close(fd);
            fd = open("/dev/digital_dis", O_WRONLY);
            if (fd < 0) { perror("reopen /dev/digital_dis"); return 1; }
        }
        tick++;
        usleep(125000);  // 125ms -> ~8 refreshes/sec, steady + bright
    }
    return 0;
}
