/** @file print.c
* Linux OS specific functions.
*/

/*
    Openptp is an open source PTP version 2 (IEEE 1588-2008) daemon.
    
    Copyright (C) 2007-2009  Flexibilis Oy

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/******************************************************************************
* $Id$
******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include <print.h>

#define DEBUG_FILE "/tmp/ptp_debug.txt"
#define DEBUG_FILE_STATE "/tmp/ptp_state.txt"

/**
* Function for printing clock status.
* @param offset_sec Offset from master in seconds.
* @param offset_nsec Offset from master in nanoseconds.
* @param drift Current drift from master.
* @param adjust Current adjustment.
* @param adjust_P P part of adjust.
* @param adjust_I I part of adjust.
*/
void print_clock_status(unsigned long offset_sec,
                        signed long offset_nsec,
                        signed long drift,
                        signed long long adjust,
                        signed long long adjust_P,
                        signed long long adjust_I)
{
    FILE *fp = 0;
    time_t tv;
    struct tm *tm = 0;
    int h = 0, m = 0, s = 0;

    /* open file */
    if (!(fp = fopen(DEBUG_FILE, "a+"))) {
        ERROR("%s\n", strerror(errno));
        return;
    }

    memset(&tv, 0, sizeof(time_t));
    time(&tv);
    tm = localtime(&tv);
    if (tm) {
        h = tm->tm_hour;
        m = tm->tm_min;
        s = tm->tm_sec;
    }

    fprintf(fp,
            "%2i:%2i.%2i %12lus %12lins %12lins/s %12llins/s %12llins/s %12llins/s\n",
            h, m, s, offset_sec, offset_nsec, drift, adjust, adjust_P,
            adjust_I);

    fclose(fp);
    return;
}

/**
* Function for printing clock state (master/slave).
* @param event State changed event.
*/
void print_clock_state(enum ptp_event_clk event)
{
    FILE *fp = 0;
    time_t tv;
    struct tm *tm = 0;
    int h = 0, m = 0, s = 0;

    /* open file */
    if (!(fp = fopen(DEBUG_FILE_STATE, "a+"))) {
        ERROR("%s\n", strerror(errno));
        return;
    }

    memset(&tv, 0, sizeof(time_t));
    time(&tv);
    tm = localtime(&tv);
    if (tm) {
        h = tm->tm_hour;
        m = tm->tm_min;
        s = tm->tm_sec;
    }
    switch (event) {
    case PTP_MASTER_CHANGED:
        fprintf(fp, "%2i:%2i.%2i PTP_MASTER_CHANGED\n", h, m, s);
        break;

    case PTP_CLK_MASTER:
        fprintf(fp, "%2i:%2i.%2i PTP_CLK_MASTER\n", h, m, s);
        break;
    default:
        fprintf(fp, "%2i:%2i.%2i UNKNOWN %i\n", h, m, s, event);
        break;
    }

    fclose(fp);
    return;
}
