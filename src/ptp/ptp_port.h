/** @file ptp_port.h
* PTP port specific stuff. 
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
#ifndef _PTP_PORT_H_
#define _PTP_PORT_H_
#include <ptp_general.h>

/**
* Port context. Given as argument to every port specific function.
*/
struct ptp_port_ctx {
    struct ptp_port_ctx *next;  ///< Internal pointer for utilizing lists.
    bool port_state_updated;    ///< flag, port state has been updated
    int timer_flags;            ///< flag for every timer enable
    struct Timestamp announce_timer;    ///< timeout for announce interval
    struct Timestamp sync_timer;        ///< timeout for sync send 
    struct Timestamp delay_req_timer;   ///< timeout for delay_req send 
    struct Timestamp pdelay_req_timer;  ///< timeout for msg send 
    struct Timestamp announce_recv_timer;
    ///< timeout for ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES
    bool announce_recv_timer_expired;
    ///< flag, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES has expired
    u16 sync_seqid;             ///< sequence id for sync
    u16 delay_req_seqid;        ///< sequence id for delay_req 
    u16 announce_seqid;         ///< sequence id for announce 
    struct Timestamp sync_recv_time;    ///< timestamp of the received sync
    u64 sync_recv_corr_field;   ///< correction field of the received sync
    struct Timestamp delay_req_send_time;       ///< timestamp of the sent delay_req
    struct ForeignMasterDataSetElem *foreign_master_head;
    ///< List head of foreign master datasets
    ClockIdentity current_master;       ///< clock identity of the current master
    bool unicast_port;          ///< flag, unicast port
    struct PortDataSet port_dataset;    ///< Port dataset      
};

/// Timer enable flags
enum PortTimerFlags {
    ANNOUNCE_TIMER = 0x01,
    SYNC_TIMER = 0x02,
    DELAY_REQ_TIMER = 0x04,
    PDELAY_REQ_TIMER = 0x08,
    ANNOUNCE_RECV_TIMER = 0x10,
};

/**
* Statemachine for PTP port.
* @param ctx Port context.
* @param current_time current time (used for port statemachine scheduling).
* @param next_time when should be called next time.
*/
void ptp_port_statemachine(struct ptp_port_ctx *ctx,
                           struct Timestamp *current_time,
                           struct Timestamp *next_time);

/**
* Function for handling received PTP messages for port.
* @param ctx Port context.
* @param buf PTP message.
* @param len msg length.
* @param time timestamp for received frame.
*/
void ptp_port_recv(struct ptp_port_ctx *ctx,
                   char *buf, int len, struct Timestamp *time);

/**
* Function for updating the PTP port state
* @param ctx Port context.
* @param new_state new statemachine state.
*/
void ptp_port_state_update(struct ptp_port_ctx *ctx,
                           enum PortState new_state);

/**
* BMC inputs for port state updating.
* @param ctx Port context.
* @param new_state new bmc input.
* @param master if BMC_SLAVE or BMC_PASSIVE, contains 
*               master ClockIdentity, otherwise NULL.
* @return true if state updated.
*/
bool ptp_port_bmc_update(struct ptp_port_ctx *ctx,
                         enum BMCUpdate bmc_update, ClockIdentity master);

/**
* Check if ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES.
* @param ctx Port context.
* @param current_time current time.
*/
void ptp_port_announce_recv_timeout_check(struct ptp_port_ctx *ctx,
                                          struct Timestamp *current_time);

/**
* Restart ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES timer (or start if stopped).
* @param ctx Port context.
* @param current_time current time.
*/
void ptp_port_announce_recv_timeout_restart(struct ptp_port_ctx *ctx, struct Timestamp
                                            *current_time);

/**
* Stop ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES timer.
* @param ctx Port context.
* @param current_time current time.
*/
void ptp_port_announce_recv_timeout_stop(struct ptp_port_ctx *ctx,
                                         struct Timestamp *current_time);

#endif                          // _PTP_PORT_H_
