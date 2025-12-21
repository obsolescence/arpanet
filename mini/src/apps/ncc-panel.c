#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "../ncc/ncp.h"

// This should match the arpanet.conf used by ncc
static const int KNOWN_IMPS[] = {1, 2, 3, 4, 5, 6, 7, 8};
static const int NUM_KNOWN_IMPS = sizeof(KNOWN_IMPS) / sizeof(KNOWN_IMPS[0]);

// Simple ASCII map placeholder
static const char *ARPANET_MAP[] = {
    "      +-------+     +-------+     +-------+",
    "      | IMP 1 |-----| IMP 2 |-----| IMP 3 |",
    "      +-------+     +-------+     +-------+",
    "          |             |             |",
    "      +-------+     +-------+     +-------+",
    "      | IMP 4 |-----| IMP 5 |-----| IMP 6 |",
    "      +-------+     +-------+     +-------+",
    "          |             |",
    "      +-------+     +-------+",
    "      | IMP 7 |-----| IMP 8 |",
    "      +-------+     +-------+",
    NULL
};

// IMP types from ncc.c
static const char *IMP_TYPE_NAMES[] = {
    "REGULAR", "LEADER_ERROR", "DOWN", "BLOCKED", "NOP", "RFNM",
    "FULL", "DEAD", "DATA_ERROR", "INCOMPL", "RESET"
};


void init_ui() {
    clear();
    
    // Draw the map
    for (int i = 0; ARPANET_MAP[i] != NULL; i++) {
        mvprintw(i, 0, ARPANET_MAP[i]);
    }

    // Draw the table headers
    int table_start_y = 15;
    mvprintw(table_start_y, 0, "IMP | Status      | Last Seen (s ago)");
    mvprintw(table_start_y + 1, 0, "----+-------------+-------------------- ");
    
    refresh();
}

void update_display(int imp_num, int status, uint64_t last_seen_tick, uint64_t current_tick) {
    int row = 17 + imp_num -1;
    long seconds_ago = (current_tick > last_seen_tick) ? (current_tick - last_seen_tick) : 0;

    const char *status_str = "UNKNOWN";
    if (status >= 0 && status < (sizeof(IMP_TYPE_NAMES) / sizeof(IMP_TYPE_NAMES[0]))) {
        status_str = IMP_TYPE_NAMES[status];
    }
    
    mvprintw(row, 0, "% -3d | % -11s | % -20ld", imp_num, status_str, seconds_ago);
}

int main(int argc, char **argv) {
    if (ncp_init(NULL) == -1) {
        fprintf(stderr, "Could not connect to ncc daemon.\n");
        exit(1);
    }

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();

    // Define colors
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);

    uint64_t current_tick = 0;

    for (;;) {
        init_ui();

        for (int i = 0; i < NUM_KNOWN_IMPS; i++) {
            int imp = KNOWN_IMPS[i];
            int status = -1;
            uint64_t last_seen = 0;
            
            if (ncp_imp_status(imp, &status, &last_seen) == 0) {
                if (i == 0) { // Use first IMP's time as a rough estimate of current tick
                    time_t now;
                    time(&now);
                    // This is not perfect, but we get the tick from the daemon's perspective
                    // A more robust solution would have the daemon return its tick
                }
                update_display(imp, status, last_seen, current_tick);
            }
        }

        refresh();
        sleep(1);
        current_tick++; // Approximation of the daemon's tick
    }

    endwin();
    return 0;
}
