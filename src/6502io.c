#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <curses.h>

#include "6502types.h"
#include "6502memory.h"
#include "6502io.h"

static framebuffer_t *display_framebuffer = NULL;

bool  cursesmode = false;
WINDOW* disp_window = NULL;
WINDOW* stat_window = NULL;
static BYTE previous_screen[1000];
static bool screen_valid = false;


bool clear_display(void) { return framebuffer_clear(display_framebuffer); }
bool write_char(BYTE x, BYTE y, const char ch) {
    return framebuffer_write_char(display_framebuffer, x, y, (BYTE)ch);
}
bool write_string(BYTE x, BYTE y, const char *text) {
    return framebuffer_write_string(display_framebuffer, x, y, text);
}
bool scroll_display(BYTE lines) { return framebuffer_scroll(display_framebuffer, lines); }
bool write_statmessage(const char* message) {
    bool ok = true;
    if (message == NULL || !cursesmode) return false;

    wmove(stat_window, 1, 1);
    wclrtoeol(stat_window);
    mvwprintw(stat_window,1,1,"%s",message);
    wnoutrefresh(stat_window);

    return ok;
}

bool display(void) {
    if (display_framebuffer == NULL) return false;
    if (!cursesmode) {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr,true);
        disp_window = newwin(27,42,(LINES-27)/2,(COLS-42)/2);
        box(disp_window,0,0);
        nodelay(disp_window,true);

        stat_window = newwin(5, 82, (LINES - 7),(COLS-82)/2);
        box(stat_window,0,0);


        cursesmode = true;
        screen_valid = false;
    }
        
    for(int y=0;y<25;y++) {
        for(int x=0;x<40;x++) {
            WORD address = (y*40)+x;
            if (!screen_valid || previous_screen[address] != framebuffer_read8(display_framebuffer, address)) {
                mvwaddch(disp_window,y+1,x+1,framebuffer_read8(display_framebuffer, address));
                previous_screen[address] = framebuffer_read8(display_framebuffer, address);
            }
        }
    }

    screen_valid = true;
    wnoutrefresh(disp_window);
    doupdate();

    return true;
}

bool backspace_pressed(void) {
    if (!cursesmode) return false;

    int ch = wgetch(disp_window);

    if (ch == KEY_BACKSPACE) return true;

    return false;
}

bool init_io(framebuffer_t *framebuffer) {
    bool ok = true;
    if (framebuffer == NULL) return false;
    display_framebuffer = framebuffer;
    screen_valid = false;
    ok = ok && clear_display();
    ok = ok && write_string(0,0,"Ready");
    ok = ok && display();

    return ok;
}

bool close_io(void) {
    if (cursesmode) {
        delwin(disp_window);
        delwin(stat_window);
        endwin();
        cursesmode = false;
    }

    display_framebuffer = NULL;
    screen_valid = false;
    return !cursesmode;
}

bool display_test(framebuffer_t *framebuffer) {
    bool ok = true;

    ok = ok && init_io(framebuffer);

    int i = 0x00;
    for(int loop=0; ok && loop<100; loop++) {
        for( int x=0; ok && x<40; x++ ) {
            for( int y=0; ok && y<25; y++ ) {
                char ch = 0x41 + (i++%26);
                ok = ok && write_char(x,y,ch);
            }
        }
        ok = ok && display();
    }

    if (!ok) {
        if (cursesmode) {
            clear_display();
            write_string(0,0,"Display test failed");
            display();
        } else {
            fprintf(stderr,"Display test failed");
        }
    } else {
        clear_display();
        write_string(0,23,"Ready");
        write_string(0,24,"Display tests succeeded");
        display();
    }

    for(BYTE l=0; l<23;l++) {
        scroll_display(1);
        display();
    }
    return ok;
}

