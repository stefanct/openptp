/** @file ptp.h
* PTP specific data. 
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
#ifndef _PTP_H_
#define _PTP_H_
#include <ptp_general.h>
#include <ptp_config.h>
#include <os_if.h>
#include <packet_if.h>
#include <clock_if.h>

#define SEC_IN_NS   1000000000

/**
* PTP daemon "master" state.
*/
enum ptp_clock_state {
    PTP_STATE_LOCAL_MASTER_CLOCK = 0,
    PTP_STATE_FOREIGN_MASTER_CLOCK,
};

/**
* PTP context. 
*/
struct ptp_ctx {
    enum ptp_clock_state clock_state;

    struct DefaultDataSet default_dataset;      ///< Default dataset for the clock
    struct CurrentDataSet current_dataset;      ///< Current dataset for the clock
    struct ParentDataSet parent_dataset;        ///< Parent dataset for the clock
    struct TimeProperitiesDataSet time_dataset;
    struct SecurityDataSet sec_dataset; ///< security dataset

    struct ptp_port_ctx *ports_list_head;       ///< List head of ports

    struct packet_ctx pkt_ctx;  ///< Packet_if context
    struct os_ctx os_ctx;       ///< Os_if context
    struct clock_ctx clk_ctx;   ///< Clock_if context

    char* ptp_cfg_file;
    char* clock_if_file;
    char* packet_if_file;
    char* os_if_file;
};

extern struct ptp_ctx ptp_ctx;
extern int socket_restart;

/**
* PTP main.
* @param argc the first argument description.
* @param argv the second argument description.
* @return The function result description.
*/
void ptp_main(int argc, char *argv[]);

// dataset initializers
void init_default_dataset(struct DefaultDataSet *dataset);
void init_current_dataset(struct CurrentDataSet *dataset);
void init_parent_dataset(struct ParentDataSet *dataset);
void init_time_dataset(struct TimeProperitiesDataSet *dataset);
void init_sec_dataset(struct SecurityDataSet *dataset);
void init_port_dataset(struct PortDataSet *dataset);

// Timestamp modification functions
void inc_timestamp(struct Timestamp *base, struct Timestamp *inc);
void dec_timestamp(struct Timestamp *base, struct Timestamp *dec);
void mult_timeout(struct Timestamp *timeout, int multiplier);
void copy_timestamp(struct Timestamp *dst, struct Timestamp *src);
int diff_timestamp(struct Timestamp *t1, struct Timestamp *t2,
                   struct Timestamp *diff);
struct Timestamp *older_timestamp(struct Timestamp *t1,
                                  struct Timestamp *t2);
void add_correction(struct Timestamp *base, s64 add);
void timeout(struct Timestamp *current, struct Timestamp *trig,
             struct Timestamp *timeout);
// For timeout calculation
u32 power2(s32 exp, u32 * nanosecs);

#endif                          // _PTP_H_
