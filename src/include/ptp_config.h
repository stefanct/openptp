/** @file ptp_config.h
* PTP configuration. 
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
#ifndef _PTP_CONFIG_H_
#define _PTP_CONFIG_H_

#include <ptp_general.h>

#define DEFAULT_CFG_FILE    "ptp_config.xml"
#define CONFIG_VERSION      "1.4"
// Protocol version
#define PTP_VERSION         2

// Maximum number of foreign master datasets that can be stored per port
// this list should cleanup if better masters arrive to the network
#define MAX_NUM_FOREIGN_MASTERS     5

// maximum number of interfaces supported
#define MAX_NUM_INTERFACES  20
#define INTERFACE_NAME_LEN  10
#define IP_STR_MAX_LEN      20

#define MAX_VALUE_LEN 100       // for parser

//#define ASYMMETRY_CORRECTION 2400
#define ASYMMETRY_CORRECTION 0

// Constants
#define DEFAULT_EVENT_PORT          319
#define DEFAULT_GENERAL_PORT        320
#define PTP_PRIMARY_MULTICAST_IP    "224.0.1.129"
#define PTP_PDELAY_MULTICAST_IP     "224.0.0.107"
#define DEFAULT_OFFSET_SCALED_LOG_VARIANCE  0xFFFF
// Number of announce messages time window
#define ANNOUNCE_WINDOW             4

struct ClockAccuracyCmp {
    char str[MAX_VALUE_LEN];
    ClockAccuracy number;
};
extern struct ClockAccuracyCmp str_to_accuracy[];
extern const unsigned int str_to_accuracy_size;

struct TimeSourceCmp {
    char str[MAX_VALUE_LEN];
    enum TimeSource time_src;
};

extern struct TimeSourceCmp str_to_source[];
extern const unsigned int str_to_source_size;

struct interface_config {
    char name[INTERFACE_NAME_LEN];
    int enabled;
    int multicast_ena;
    int num_unicast_addr;
    char unicast_ip[MAX_NUM_INTERFACES][IP_STR_MAX_LEN];
};

struct ptp_config {
    int custom_clk_if;
    int debug;
    int clock_status_file;
    int num_interfaces;
    struct interface_config interfaces[MAX_NUM_INTERFACES];
    int one_step_clock;
    int clock_class;
    int clock_accuracy;
    int clock_priority1;
    int clock_priority2;
    int clock_source;
    int domain;
    int announce_interval;
    int sync_interval;
    int delay_req_interval;
    /* The DEFAULT_DELAY_REQ_INTERVAL value shall be an integer 
     * with the minimum value being
     * DEFAULT_SYNC_INTERVAL and a maximum 
     * value of DEFAULT_SYNC_INTERVAL +5. */
};
extern struct ptp_config ptp_cfg;

int read_initialization(char *filename);

#endif                          // _PTP_CONFIG_H_
