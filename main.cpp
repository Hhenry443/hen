/*** includes ***/

#include "iostream"
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

/*** defines ***/
// This macro basically sets the upper 3 binary digits of k to 0. This mirrors what holding control and pressing
// k would do! This is how we check for pressing control+k or whatever char!
#define CTRL_KEY(k) ((k) & 0x1f)
#define HEN_VERSION "0.0.1"

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DELETE,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};
// using https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

/*** data ***/
struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios orig_termios; // Keep the original terminal attributes
};

struct editorConfig E;

/*** terminal ***/
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr"); // get the current attributes into orig termios
    atexit(disableRawMode); // function called when program exits

    struct termios raw = E.orig_termios; // new termios struct to make changes in

    // We do a bitwise and to flip the flags!  Think of it like a long binary string. doing an &= can flip the bits!

    // Local Flags:
    // ECHO - Turn off outputting the input characters. We want to process them!
    // ICANON - Turn off canonical mode, we want to take input as it comes
    // ISIT - This stops things like signal interrupt, so we can press ctrl c and it doesnt quit
    // IEXTEN - This prevents the ctrl v input .
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    // Input Flags
    // IXON - This stops the software flow control keys, ctrl s and ctrl q (pause and continue input)
    // ICRNL - Fixes control M
    // BRKINT - Another prevention for ending the program
    // INPCK - Enable parity checcking
    // ISTRIP - Strip the 8th bit
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);

    // Output Flags
    // OPOST - This stops the program from doing a carriage return followed by a newline.
    raw.c_oflag &= ~(OPOST);

    // This is not a flag
    raw.c_cflag |= (CS8);

    // Minimum number of characters for noncanonical read
    raw.c_cc[VMIN] = 0;

    // Deciseconds until the read() returns
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr"); // apply the new flags to the terminal
}

// Read a key, check all went ok, return it
int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DELETE;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

}

/*** append buffer ***/

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *ns = (char *)realloc(ab->b, ab->len + len);

    if (ns == NULL) return;

    memcpy(&ns[ab->len], s, len);
    ab->b = ns;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}
/*** input ***/
void editorMoveCursor(int key) {
    // W and S are reversed.
    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) { E.cx--; }
            break;
        case ARROW_RIGHT:
            if (E.cx != E.screencols -1) { E.cx++; }
            break;
        case ARROW_UP:
            if (E.cy != 0) { E.cy--; }
            break;
        case ARROW_DOWN:
            if (E.cy != E.screenrows - 1) { E.cy++; }
            break;
        default:
            perror("Something went wrong.");
            exit(1);
            break;
    }
}
// Process the key read from editorReadKey
void editorProcessKey() {
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
            break;

        case PAGE_UP:
        case PAGE_DOWN: {
            int times = E.screenrows;
            while (times--) editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        } break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            E.cx = E.screencols - 1;
            break;

        default:
            std::cout << c << std::flush;
            break;
    }
}

/*** output ***/
void editorDrawRows(struct abuf* ab) {
    int y;
    for (int y = 0; y < E.screenrows; ++y) {
        if (y == E.screenrows / 3) {
            char welcome[800];
            int welcomelen = snprintf(welcome, sizeof(welcome), "Hen editor -- version %s", HEN_VERSION);
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;

            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
        }

        abAppend(ab, "\x1b[K", 3); // Escape sequence to clear a line

        if (y < E.screenrows -1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // Escape code for hiding the cursor
    abAppend(&ab, "\x1b[H", 3); // Escape sequence to position the cursor in the top corner

    editorDrawRows(&ab);

    char buf[32];
    // Put the buffer at the position of the cursor using the H escape code. We format this using snprintf into the buffer
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6); // Escape code for showing it again

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** init ***/
void initEditor() {
    E.cx = 0;
    E.cy = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}
// We want to make a terminal text editor.
// This will be like vim or vi. Maybe just nano to start
int main() {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKey();
    }

    return 0;
}