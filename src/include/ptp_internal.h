/** @file ptp_internal.h
* Internal definitions.
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
#ifndef _PTP_INTERNAL_H_
#define _PTP_INTERNAL_H_

#include <linux/ptp_types.h>
#include <ptp_config.h>
#include <ptp_message.h>
#include <clock_if.h>

// Constants
#define MAX_PTP_FRAME_SIZE 200

/**
* Foreign master dataset.
*/
struct ForeignMasterDataSet {
    struct PortIdentity src_port_id;    ///< sourcePortIdentity from announce message
    char src_ip[IP_STR_MAX_LEN];        ///< source ip from announce msg
    struct PortIdentity dst_port_id;    ///< sourcePortIdentity of the receiver of the announce message
    u8 foreign_master_announce_messages;        ///< number of annouce messages received during FOREIGN_MASTER_TIME_WINDOW
    u8 tstamp_index;            ///< wr index for announce_tstamp
    struct Timestamp announce_tstamp[ANNOUNCE_WINDOW];  ///< timestamps for the announce messages during recv window
    struct ptp_announce msg;    ///< stored announce message (one per src_port_id)
};

/**
* Foreign master dataset linked list element. 
*/
struct ForeignMasterDataSetElem {
    struct ForeignMasterDataSetElem *next;      ///< for linked list of foreign masters
    struct ForeignMasterDataSet *data_p;        ///< pointer to data (to be identical to ForeignMasterDataSetElem_p)
    struct ForeignMasterDataSet data;   ///< dataset
};

/**
* Foreign master dataset pointer linked list struct. 
*/
struct ForeignMasterDataSetElem_p {
    struct ForeignMasterDataSetElem_p *next;    ///< for linked list of foreign masters
    struct ForeignMasterDataSet *data_p;        ///< pointer to data
};

// Helpers
inline static u64 ntohll(u64 x)
{
    u32 *tmp1 = (u32 *) & x;
    u32 *tmp2 = (tmp1 + 1);
    u64 tmp3 = (((u64) (ntohl(*tmp1))) << 32) & 0xffffffff00000000ll;
    tmp3 |= 0xffffffff & ntohl(*tmp2);
    return tmp3;
}

#define htonll(x) ntohll(x)

// String helpers
char *get_ptp_event_clk_str(enum ptp_event_clk event);
char *get_ptp_event_ctrl_str(enum ptp_event_ctrl event);
char *get_state_str(enum PortState state);
char *get_bmc_update_str(enum BMCUpdate update);

// port and clock id comparing functions
int compare_clock_id(ClockIdentity id1, ClockIdentity id2);

#endif                          // _PTP_INTERNAL_H_
