#include "iostream"
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

// using https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

struct termios orig_termios; // Keep the original terminal attributes

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // On exit we reset the terminal with its og self
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios); // get the current attributes into orig termios
    atexit(disableRawMode); // function called when program exits

    struct termios raw = orig_termios; // new termios struct to make changes in

    // c_lfag is a place for misc flags
    // We do a bitwise and with a flipped echo. This basically just flips the bit of echo to NOT it
    // Stop the terminal from outputting our input (ECHO)
    // and switch it out of cannonical mode processing every keystroke as it comes
    raw.c_lflag &= ~(ECHO | ICANON);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // apply the new flags to the terminal
}

// We want to make a terminal text editor.
// This will be like vim or vi. Maybe just nano to start
int main() {
    enableRawMode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if (iscntrl(c)) {
            std::cout << c << '\n';
        } else {
            std::cout << (int)c << " ('" << c << "')" << '\n';
        }
    };

    return 0;
}