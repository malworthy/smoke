/**
 Linux (POSIX) implementation of _kbhit().
 Morgan McGuire, morgan@cs.brown.edu
 */
#ifndef _WIN32

#include <stdio.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stropts.h>
#include <stdbool.h>
#include <unistd.h>

struct termios orig_settings;
static bool initialized = false;
static const int STDIN = 0;

void restoreTerminal()
{
    if(initialized)
        tcsetattr(STDIN,TCSANOW,&orig_settings);
}

int _kbhit() {

    if (! initialized) {
        // Use termios to turn off line buffering
        struct termios term;
        tcgetattr(STDIN, &term);
        
        orig_settings = term;

        term.c_lflag &= (~ICANON & ~ECHO);
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = true;
        fflush(stdout);
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

int _getch()
{
    return getchar();
}

#endif
