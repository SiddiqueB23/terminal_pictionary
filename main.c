#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

int NROWS, NCOLS;
int MOUSEX, MOUSEY;

enum KEY_ACTION {
    KEY_NULL = 0,    /* NULL */
    CTRL_C = 3,      /* Ctrl-c */
    CTRL_D = 4,      /* Ctrl-d */
    CTRL_F = 6,      /* Ctrl-f */
    CTRL_H = 8,      /* Ctrl-h */
    TAB = 9,         /* Tab */
    CTRL_L = 12,     /* Ctrl+l */
    ENTER = 13,      /* Enter */
    CTRL_Q = 17,     /* Ctrl-q */
    CTRL_S = 19,     /* Ctrl-s */
    CTRL_U = 21,     /* Ctrl-u */
    ESC = 27,        /* Escape */
    BACKSPACE = 127, /* Backspace */
    /* The following are just soft codes, not really reported by the
     * terminal directly. */
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,

    LMB_DOWN,
    LMB_UP,
    LMB_PRESSED_MOVE,
    MMB_DOWN,
    MMB_UP,
    MMB_PRESSED_MOVE,
    RMB_DOWN,
    RMB_UP,
    RMB_PRESSED_MOVE,
    SCROLL_UP,
    SCROLL_DOWN,
    MOUSE_MOVE
};

void printKeyAction(int k) {
    switch (k) {
    case KEY_NULL:
        printf("KEY_NULL");
        break;
    case CTRL_C:
        printf("CTRL_C");
        break;
    case CTRL_D:
        printf("CTRL_D");
        break;
    case CTRL_F:
        printf("CTRL_F");
        break;
    case CTRL_H:
        printf("CTRL_H");
        break;
    case TAB:
        printf("TAB");
        break;
    case CTRL_L:
        printf("CTRL_L");
        break;
    case ENTER:
        printf("ENTER");
        break;
    case CTRL_Q:
        printf("CTRL_Q");
        break;
    case CTRL_S:
        printf("CTRL_S");
        break;
    case CTRL_U:
        printf("CTRL_U");
        break;
    case ESC:
        printf("ESC");
        break;
    case BACKSPACE:
        printf("BACKSPACE");
        break;
    case ARROW_LEFT:
        printf("ARROW_LEFT");
        break;
    case ARROW_RIGHT:
        printf("ARROW_RIGHT");
        break;
    case ARROW_UP:
        printf("ARROW_UP");
        break;
    case ARROW_DOWN:
        printf("ARROW_DOWN");
        break;
    case DEL_KEY:
        printf("DEL_KEY");
        break;
    case HOME_KEY:
        printf("HOME_KEY");
        break;
    case END_KEY:
        printf("END_KEY");
        break;
    case PAGE_UP:
        printf("PAGE_UP");
        break;
    case PAGE_DOWN:
        printf("PAGE_DOWN");
        break;
    case LMB_DOWN:
        printf("LMB_DOWN");
        break;
    case LMB_UP:
        printf("LMB_UP");
        break;
    case LMB_PRESSED_MOVE:
        printf("LMB_PRESSED_MOVE");
        break;
    case MMB_DOWN:
        printf("MMB_DOWN");
        break;
    case MMB_UP:
        printf("MMB_UP");
        break;
    case MMB_PRESSED_MOVE:
        printf("MMB_PRESSED_MOVE");
        break;
    case RMB_DOWN:
        printf("RMB_DOWN");
        break;
    case RMB_UP:
        printf("RMB_UP");
        break;
    case RMB_PRESSED_MOVE:
        printf("RMB_PRESSED_MOVE");
        break;
    case SCROLL_UP:
        printf("SCROLL_UP");
        break;
    case SCROLL_DOWN:
        printf("SCROLL_DOWN");
        break;
    case MOUSE_MOVE:
        printf("MOUSE_MOVE");
        break;
    default:
        printf("%c", k);
        break;
    }
}

/* ======================= Low level terminal handling ====================== */

static struct termios orig_termios; /* In order to restore at exit.*/

void disableRawMode(int fd) {
    tcsetattr(fd, TCSAFLUSH, &orig_termios);
}

/* Called at exit to avoid remaining in raw mode. */
void termAtExit(void) {
    disableRawMode(STDIN_FILENO);
}

/* Raw mode: 1960 magic shit. */
int enableRawMode(int fd) {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) goto fatal;
    atexit(termAtExit);
    if (tcgetattr(fd, &orig_termios) == -1) goto fatal;

    raw = orig_termios; /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - echoing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    // raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    /* control chars - set return condition: min number of bytes and timer. */
    raw.c_cc[VMIN] = 0;  /* Return each byte, or zero for timeout. */
    raw.c_cc[VTIME] = 1; /* 100 ms timeout (unit is tens of second). */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) goto fatal;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

/* Read a key from the terminal put in raw mode, trying to handle
 * escape sequences. */
int termReadKey(int fd) {
    int nread;
    char c, seq[32];
    if ((nread = read(fd, &c, 1)) == 0) return KEY_NULL;
    if (nread == -1) exit(1);

    switch (c) {
    case ESC: /* escape sequence */
        /* If this is just an ESC, we'll timeout here. */
        if (read(fd, seq, 1) == 0) return ESC;
        if (read(fd, seq + 1, 1) == 0) return ESC;

        // printf("%d", seq[1]);
        // fflush(stdout);
        /* ESC [ sequences. */
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                /* Extended escape, read additional byte. */
                if (read(fd, seq + 2, 1) == 0) return ESC;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                    case '3':
                        return DEL_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    }
                }
            } else if (seq[1] == '<') {
                int i = 2;
                while (i < 31) {
                    if (read(STDIN_FILENO, seq + i, 1) != 1) break;
                    if (seq[i] == 'M' || seq[i] == 'm') break;
                    i++;
                }

                seq[i + 1] = '\0';
                int type, x, y;
                if (sscanf(seq + 1, "<%d;%d;%d", &type, &x, &y) == 3) {
                    // printf("x:%d,y:%d", x, y);
                    // fflush(stdout);
                    MOUSEX = x;
                    MOUSEY = y;
                    // printf("\x1b[%d;%dH%d", MOUSEY, MOUSEX, type);
                    // fflush(stdout);
                }
                switch (type) {
                case 0:
                    if (seq[i] == 'M')
                        return LMB_DOWN;
                    else if (seq[i] == 'm')
                        return LMB_UP;
                    break;
                case 1:
                    if (seq[i] == 'M')
                        return MMB_DOWN;
                    else if (seq[i] == 'm')
                        return MMB_UP;
                    break;
                case 2:
                    if (seq[i] == 'M')
                        return RMB_DOWN;
                    else if (seq[i] == 'm')
                        return RMB_UP;
                    break;
                case 32:
                    if (seq[i] == 'M')
                        return LMB_PRESSED_MOVE;
                    break;
                case 33:
                    if (seq[i] == 'M')
                        return MMB_PRESSED_MOVE;
                    break;
                case 34:
                    if (seq[i] == 'M')
                        return RMB_PRESSED_MOVE;
                    break;
                case 35:
                    return MOUSE_MOVE;
                case 64:
                    return SCROLL_UP;
                case 65:
                    return SCROLL_DOWN;
                }
                // printf("%s", seq + 1);
                // fflush(stdout);
                return ESC;
            } else {
                switch (seq[1]) {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        }

        /* ESC O sequences. */
        else if (seq[0] == 'O') {
            switch (seq[1]) {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
            }
        }
        break;
    }
    return c;
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor is stored at *rows and *cols and 0 is returned. */
int getCursorPosition(int ifd, int ofd, int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf) - 1) {
        if (read(ifd, buf + i, 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf + 2, "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

/* Try to get the number of columns in the current terminal. If the ioctl()
 * call fails the function will try to query the terminal itself.
 * Returns 0 on success, -1 on error. */
int getWindowSize(int ifd, int ofd, int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int orig_row, orig_col, retval;

        /* Get the initial position so we can restore it later. */
        retval = getCursorPosition(ifd, ofd, &orig_row, &orig_col);
        if (retval == -1) goto failed;

        /* Go to right/bottom margin and get position. */
        if (write(ofd, "\x1b[999C\x1b[999B", 12) != 12) goto failed;
        retval = getCursorPosition(ifd, ofd, rows, cols);
        if (retval == -1) goto failed;

        /* Restore position. */
        char seq[32];
        snprintf(seq, 32, "\x1b[%d;%dH", orig_row, orig_col);
        if (write(ofd, seq, strlen(seq)) == -1) {
            /* Can't recover... */
        }
        return 0;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

failed:
    return -1;
}

/* ========================= Canvas  ======================== */

struct canvas {
    int starty, startx;
    int sizey, sizex;
    int *colorBuf;
};

#define CANVAS_INIT {1, 1, 3, 3, NULL}

struct canvas mainCanvas = CANVAS_INIT;

int brushTemplates[5][9][9] = {
    {
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 1, 1, 1, 0, 0, 0},
        {0, 0, 0, 1, 1, 1, 0, 0, 0},
        {0, 0, 0, 1, 1, 1, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 1, 1, 1, 0, 0, 0},
        {0, 0, 1, 1, 1, 1, 1, 0, 0},
        {0, 0, 1, 1, 1, 1, 1, 0, 0},
        {0, 0, 1, 1, 1, 1, 1, 0, 0},
        {0, 0, 0, 1, 1, 1, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 1, 1, 1, 0, 0, 0},
        {0, 0, 1, 1, 1, 1, 1, 0, 0},
        {0, 1, 1, 1, 1, 1, 1, 1, 0},
        {0, 1, 1, 1, 1, 1, 1, 1, 0},
        {0, 1, 1, 1, 1, 1, 1, 1, 0},
        {0, 0, 1, 1, 1, 1, 1, 0, 0},
        {0, 0, 0, 1, 1, 1, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
    },
    {
        {0, 0, 1, 1, 1, 1, 1, 0, 0},
        {0, 1, 1, 1, 1, 1, 1, 1, 0},
        {1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1},
        {0, 1, 1, 1, 1, 1, 1, 1, 0},
        {0, 0, 1, 1, 1, 1, 1, 0, 0},
    },

};

int selectedColor = 0;
int brushSize = 1;
int minBrushSize = 1, maxBrushSize = 5;
int fillMode = 0;

void initializeCanvas(struct canvas *c) {
    c->colorBuf = realloc(c->colorBuf, (c->sizey - 2) * (c->sizex - 2) * sizeof(int));
    if (c->colorBuf == NULL) exit(0);
    for (int i = 0; i < (c->sizey - 2) * (c->sizex - 2); i++)
        c->colorBuf[i] = 15;
}

int setPixel(struct canvas *c, int y, int x, int color) {
    if (y < 0 || y > c->sizey - 3 || x < 0 || x > c->sizex - 3) return -1;
    c->colorBuf[y * (c->sizex - 2) + x] = color;
    return 0;
}

int getPixel(struct canvas *c, int y, int x) {
    if (y < 0 || y > c->sizey - 3 || x < 0 || x > c->sizex - 3) return -1;
    return c->colorBuf[y * (c->sizex - 2) + x];
}

int translateCanvasPosition(struct canvas *c, int y, int x, int *cy, int *cx) {
    *cy = y - c->starty - 1;
    *cx = x - c->startx - 1;
    if (*cy < 0 || *cy > c->sizey - 3 || *cx < 0 || *cx > c->sizex - 3) return -1;
    return 0;
}

int isBrushPixel(int y, int x) {
    int mcy, mcx;
    translateCanvasPosition(&mainCanvas, MOUSEY, MOUSEX, &mcy, &mcx);
    int by = y - (mcy - 4);
    int bx = x - (mcx - 4);
    if (bx < 0 || bx > 8 || by < 0 || by > 8) return 0;
    return brushTemplates[brushSize - 1][by][bx];
}

int canvasRefreshScreen(struct canvas *c) {
    if (c->startx < 1 || c->starty < 1) {
        return -1;
    }

    printf("\x1b[%d;%dH", c->starty, c->startx);
    printf("╔");
    for (int j = c->startx + 1; j < NCOLS && j < c->startx + c->sizex - 1; j++)
        printf("═");
    printf("╗");
    int cy, cx, color = -1;
    for (int i = c->starty + 1; i < NROWS && i < c->starty + c->sizey - 1; i++) {
        printf("\x1b[%d;%dH", i, c->startx);
        printf("║");
        for (int j = c->startx + 1; j < NCOLS && j < c->startx + c->sizex - 1; j++) {
            translateCanvasPosition(&mainCanvas, i, j, &cy, &cx);
            if (fillMode && MOUSEY == i && MOUSEX == j)
                printf("\x1b[48;5;%dm\x1b[38;5;%dmU\x1b[0m", selectedColor, (selectedColor < 7 ? 15 : 0));
            else if (!fillMode && isBrushPixel(cy, cx))
                printf("\x1b[48;5;%dm\x1b[38;5;%dm▒\x1b[0m", selectedColor, (selectedColor < 15 ? 15 : 7));
            else {
                color = getPixel(&mainCanvas, cy, cx);
                if (color < 0 || color > 15)
                    printf("#");
                else
                    printf("\x1b[48;5;%dm ", color);
            }
        }
        printf("\x1b[0m");
        printf("║");
    }
    printf("\x1b[%d;%dH", c->starty + c->sizey - 1, c->startx);
    printf("╚");
    for (int j = c->startx + 1; j < NCOLS && j < c->startx + c->sizex - 1; j++)
        printf("═");
    printf("╝");

    fflush(stdout);

    return 0;
}

int fillCanvas(struct canvas *c, int y, int x, int old_color, int new_color) {
    if (y < 0 || y > c->sizey - 2 || x < 0 || x > c->sizex - 2) return 0;
    if (getPixel(c, y, x) != old_color) return 0;
    setPixel(c, y, x, new_color);
    return 1 +
           fillCanvas(c, y + 1, x, old_color, new_color) +
           fillCanvas(c, y - 1, x, old_color, new_color) +
           fillCanvas(c, y, x + 1, old_color, new_color) +
           fillCanvas(c, y, x - 1, old_color, new_color);
}

/* ========================= Toolbar  ======================== */

int toolbarColors[22] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 7, 7, 7, 7, 7, 7};
int toolbarSelected[22] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
int toolbarPressed[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char selectedChar[3][3][4] = {
    {"╔", "═", "╗"},
    {"║", " ", "║"},
    {"╚", "═", "╝"},
};
char hoveredChar[3][3][4] = {
    {"┌", "─", "┐"},
    {"│", " ", "│"},
    {"└", "─", "┘"},
};

char toolbarIcons[6][4] = {"/", "U", "X", "-", " ", "+"};

void toolbarRefreshScreen() {
    for (int i = 0; i < 3; i++) {
        printf("\x1b[%d;%dH", 64 + i, (NCOLS - 3 * 22) / 2);
        for (int j = 0; j < 22; j++) {
            printf("\x1b[48;5;%dm", toolbarColors[j]);
            if (toolbarColors[j] < 7)
                printf("\x1b[38;5;15m");
            else
                printf("\x1b[38;5;0m");
            for (int k = 0; k < 3; k++) {
                if (i == 1 && k == 1) {
                    if (j < 16)
                        printf(" ");
                    else if (j == 20)
                        printf("%d", brushSize);
                    else
                        printf("%s", toolbarIcons[j - 16]);
                } else if (toolbarPressed[j]) {
                    printf("█");
                } else if (toolbarSelected[j]) {
                    printf("%s", selectedChar[i][k]);
                } else if (MOUSEY >= 64 && MOUSEY <= 66 &&
                           (NCOLS - 3 * 22) / 2 + 3 * j <= MOUSEX &&
                           (NCOLS - 3 * 22) / 2 + 3 * j + 2 >= MOUSEX &&
                           j != 20)
                    printf("%s", hoveredChar[i][k]);
                else
                    printf(" ");
            }
            if (toolbarPressed[j] && i == 2) toolbarPressed[j] = 0;
        }
        printf("\x1b[0m");
    }
    fflush(stdout);
}

/* ============================= Terminal update ============================ */

void drawMouse(void) {
    printf("\x1b[%d;%dH▒", MOUSEY, MOUSEX);
}

void termRefreshScreen(void) {
    printf("\x1b[?25l"); /* Hide cursor. */
    // write(STDOUT_FILENO, "\x1b[2J", 4); /* Clear screen */
    write(STDOUT_FILENO, "\x1b[H", 3); /* Move cursor to home */
    // mainCanvas.startx = MOUSEX;
    // mainCanvas.starty = MOUSEY;

    // printf(" %d %d ", cx, cy);
    // fflush(stdout);
    canvasRefreshScreen(&mainCanvas);
    toolbarRefreshScreen();
    // drawMouse();

    // printf("\x1b[%d;%dH", MOUSEY, MOUSEX);
    // printf("H");
    // fflush(stdout);
    // getMousePosition(STDIN_FILENO, STDOUT_FILENO);
    // int y, x;
    // getCursorPosition(STDIN_FILENO, STDOUT_FILENO, &y, &x);
    // printf("ROWS:%d COLS: %d;  ", NROWS, NCOLS);
    // fflush(stdout);
}

/* ========================= Term events handling  ======================== */

/* Process events arriving from the standard input, which is, the user
 * is typing stuff on the terminal. */
void termProcessKeypress(int fd) {
    int c = termReadKey(fd);
    // write(STDOUT_FILENO, "\x1b[H", 3); /* Move cursor to home */
    // printKeyAction(c);
    // printf("              ");
    // fflush(stdout);
    int cy, cx, toolbarBtnPressed, old_color;
    switch (c) {
    case ENTER: /* Enter */
        break;
    case CTRL_C: /* Ctrl-c */
        break;
    case CTRL_Q: /* Ctrl-q */
        write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
        write(STDOUT_FILENO, "\x1b[?25h", 6); /* Show cursor. */

        write(STDOUT_FILENO, "\x1b[?1006l", 8);
        write(STDOUT_FILENO, "\x1b[?1003l", 8);
        write(STDOUT_FILENO, "\x1b[?1015l", 8);
        exit(0);
        break;
    case CTRL_S: /* Ctrl-s */
        break;
    case CTRL_F:
        break;
    case BACKSPACE: /* Backspace */
    case CTRL_H:    /* Ctrl-h */
    case DEL_KEY:
        break;
    case PAGE_UP:
    case PAGE_DOWN:
        break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        break;
    case CTRL_L: /* ctrl+l*/
        break;
    case ESC:
        break;
    case LMB_DOWN:
    case LMB_PRESSED_MOVE:
        toolbarBtnPressed = (MOUSEX - (NCOLS - 3 * 22) / 2) / 3;
        if (64 <= MOUSEY && MOUSEY <= 66 && toolbarBtnPressed >= 0 && toolbarBtnPressed < 22) {
            if (toolbarBtnPressed != 20)
                toolbarPressed[toolbarBtnPressed] = 1;
            if (toolbarBtnPressed < 16) {
                for (int i = 0; i < 16; i++)
                    toolbarSelected[i] = 0;
                selectedColor = toolbarBtnPressed;
                toolbarSelected[toolbarBtnPressed] = 1;
            } else if (toolbarBtnPressed == 16) {
                fillMode = 0;
                toolbarSelected[17] = 0;
                toolbarSelected[toolbarBtnPressed] = 1;
            } else if (toolbarBtnPressed == 17) {
                fillMode = 1;
                toolbarSelected[16] = 0;
                toolbarSelected[toolbarBtnPressed] = 1;
            } else if (toolbarBtnPressed == 18) {
                initializeCanvas(&mainCanvas);
            } else if (toolbarBtnPressed == 19) {
                brushSize--;
                if (brushSize < minBrushSize) brushSize = minBrushSize;
            } else if (toolbarBtnPressed == 21) {
                brushSize++;
                if (brushSize > maxBrushSize) brushSize = maxBrushSize;
            }
        }
        if (translateCanvasPosition(&mainCanvas, MOUSEY, MOUSEX, &cy, &cx) != -1) {
            if (fillMode) {
                old_color = getPixel(&mainCanvas, cy, cx);
                if (old_color != selectedColor)
                    fillCanvas(&mainCanvas, cy, cx, old_color, selectedColor);
            } else {
                for (int i = 0; i < 9; i++) {
                    for (int j = 0; j < 9; j++) {
                        if (brushTemplates[brushSize - 1][i][j])
                            setPixel(&mainCanvas, cy - 4 + i, cx - 4 + j, selectedColor);
                    }
                }
            }
        }
        break;
    case SCROLL_UP:
        brushSize++;
        if (brushSize > maxBrushSize) brushSize = maxBrushSize;
        break;
    case SCROLL_DOWN:
        brushSize--;
        if (brushSize < minBrushSize) brushSize = minBrushSize;
        break;
    default:
        break;
    }
}

void updateWindowSize(void) {
    if (getWindowSize(STDIN_FILENO, STDOUT_FILENO,
                      &NROWS, &NCOLS) == -1) {
        perror("Unable to query the screen for size (columns / rows)");
        exit(1);
    }
}

void handleSigWinCh(int unused __attribute__((unused))) {
    updateWindowSize();
    termRefreshScreen();
}

// void clean_exit_on_sig(int sig_num) {
//     printf("\n Signal %d received", sig_num);
// }

void initTerm(void) {
    updateWindowSize();
    signal(SIGWINCH, handleSigWinCh);
    // signal(SIGSEGV, clean_exit_on_sig);
}

int main() {

    initTerm();
    enableRawMode(STDIN_FILENO);

    mainCanvas.sizex = 82;
    mainCanvas.sizey = 62;
    mainCanvas.startx = (NCOLS - mainCanvas.sizex) / 2;
    mainCanvas.starty = 1;
    initializeCanvas(&mainCanvas);

    write(STDOUT_FILENO, "\x1b[2J", 4); /* Clear screen */
    write(STDOUT_FILENO, "\x1b[H", 3);  /* Move cursor to home */

    /* Enable Mouse reporting*/
    write(STDOUT_FILENO, "\x1b[?1006h", 8);
    write(STDOUT_FILENO, "\x1b[?1003h", 8);
    write(STDOUT_FILENO, "\x1b[?1015h", 8);

    printf("\x1b[48;5;7m\x1b[38;5;8m");
    for (int i = 0; i < NROWS * NCOLS; i++) {
        if (rand() % 2)
            printf("🮘");
        else
            printf("🮙");
    }
    printf("\x1b[0m");
    fflush(stdout);

    termProcessKeypress(STDIN_FILENO);
    while (1) {
        termRefreshScreen();
        termProcessKeypress(STDIN_FILENO);
    }
    return 0;
}