/** @file ptp_framer.h
* PTP framer header. 
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
#ifndef _PTP_FRAMER_H_
#define _PTP_FRAMER_H_
#include <ptp_general.h>
#include "ptp_port.h"

/**
* Function for creating PTP Sync message.
* @param ctx Port context.
* @param buf Buffer to which frame is created.
* @param seqid Sequence id.
* @return size of the created frame.
*/
int create_sync(struct ptp_port_ctx *ctx, char *buf, u16 seqid);

/**
* Function for creating PTP Follow_Up message.
* @param ctx Port context.
* @param buf Buffer to which frame is created.
* @param time when sync was sent
* @param seqid Sequence id.
* @return size of the created frame.
*/
int create_follow_up(struct ptp_port_ctx *ctx, char *buf,
                     struct Timestamp *time, u16 seqid);

/**
* Function for creating PTP Announce message.
* @param ctx Port context.
* @param buf Buffer to which frame is created.
* @param seqid Sequence id.
* @param local if true, use data from defaut dataset (instead of parent).
* @return size of the created frame.
*/
int create_announce(struct ptp_port_ctx *ctx, char *buf, u16 seq_id, int local);

/**
* Function for creating PTP Delay_Req message.
* @param ctx Port context.
* @param buf Buffer to which frame is created.
* @param seqid Sequence id.
* @return size of the created frame.
*/
int create_delay_req(struct ptp_port_ctx *ctx, char *buf, u16 seqid);

/**
* Function for creating PTP Delay_Resp message.
* @param ctx Port context.
* @param buf Buffer to which frame is created.
* @param time when delay_req was recvd
* @param Source port of Delay_req
* @param seqid Sequence id.
* @param corr_field Correction field of the Delay_req
* @return size of the created frame.
*/
int create_delay_resp(struct ptp_port_ctx *ctx, char *buf,
                      struct Timestamp *time,
                      struct PortIdentity *src_port_id,
                      u16 seqid, u64 corr_field);

#endif                          // _PTP_FRAMER_H_
