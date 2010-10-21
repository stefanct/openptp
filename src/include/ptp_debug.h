/** @file ptp_debug.h
* PTP debug declarations. 
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
#ifndef _PTP_DEBUG_H_
#define _PTP_DEBUG_H_

#include <syslog.h>

#include <ptp_config.h>

#define DEBUG(x...) \
    do { \
        if(ptp_cfg.debug) { \
            OUTPUT_SYSLOG(LOG_DEBUG, ##x); \
        } \
    } while(0)

#define ERROR(x...) \
    do { \
        OUTPUT_SYSLOG(LOG_ERR, ##x); \
    } while(0)

/// Output a message to syslog. Not meant to be used directly.
#define OUTPUT_SYSLOG(prio,fmt,x...) \
    syslog(LOG_DAEMON | prio, "%s:%i %s: " fmt, \
            __FILE__, __LINE__, __FUNCTION__, ##x); \

#define DEBUG_PLAIN(fmt,x...) DEBUG("[PLAIN] ", fmt, ##x)

static char tmp_str[40];
inline static char *ptp_clk_id(u8 * clk_id)
{
    snprintf(tmp_str, 40, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             0xff & clk_id[0], 0xff & clk_id[1], 0xff & clk_id[2],
             0xff & clk_id[3], 0xff & clk_id[4], 0xff & clk_id[5],
             0xff & clk_id[6], 0xff & clk_id[7]);
    return tmp_str;
}

inline static void ptp_dump(u8 * str, int len)
{
    int i = 0;
    printf("DUMP: ");
    for (i = 0; i < len; i++) {
        printf("%02x ", str[i]);
        if ((i != 0) && ((i + 1) % 8 == 0))
            printf("\n      ");
    }
    printf("\n");
}

#endif                          // _PTP_DEBUG_H_
