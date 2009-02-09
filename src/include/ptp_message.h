/** @file ptp_message.h
* PTP message formats. 
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
#ifndef _PTP_MESSAGE_H_
#define _PTP_MESSAGE_H_

/// PTP message types.
#define PTP_SYNC                    0x0 ///< Sync
#define PTP_DELAY_REQ               0x1 ///< Delay_Req
#define PTP_PDELAY_REQ              0x2 ///< Pdelay_Req
#define PTP_PDELAY_RESP             0x3 ///< Pdelay_Resp
#define PTP_FOLLOW_UP               0x8 ///< Follow_Up
#define PTP_DELAY_RESP              0x9 ///< Delay_Resp
#define PTP_PDELAY_RESP_FOLLOW_UP   0xA ///< Pdelay_Resp_Follow_Up
#define PTP_ANNOUNCE                0xB ///< Announce
#define PTP_SIGNALING               0xC ///< Signaling
#define PTP_MANAGEMENT              0xD ///< Management

/// PTP message flags.
#define PTP_ALTERNATE_MASTER        0x0001      ///<
#define PTP_TWO_STEP                0x0002      ///<
#define PTP_UNICAST                 0x0004      ///<
#define PTP_PROFILE1                0x0020      ///<
#define PTP_PROFILE2                0x0040      ///<
#define PTP_LI_61                   0x0100      ///<
#define PTP_LI_59                   0x0200      ///<
#define PTP_UTC_OFFSET_VALID        0x0400      ///<  The value of current_utc_offset_valid
#define PTP_TIMESCALE               0x0800      ///<  The value of ptp_timescale of the time properties data set.
#define PTP_TIME_TRACEABLE          0x1000      ///<  The value of time_traceable of the time properties data set.
#define PTP_FREQ_TRACEABLE          0x1000      ///<  The value of frequency_traceable of the time properties data set.

/// PTP header control field values (for backward combatibility).
#define PTP_CTRL_SYNC               0x0 ///< Sync
#define PTP_CTRL_DELAY_REQ          0x1 ///< Delay_Resp
#define PTP_CTRL_FOLLOW_UP          0x2 ///< Follow_Up
#define PTP_CTRL_DELAY_RESP         0x3 ///< Delay_Resp
#define PTP_CTRL_MANAGEMENT         0x4 ///< Management
#define PTP_CTRL_OTHER              0x5 ///< All other messages

/** 
* Default value for logMeanMessageInterval 
* (for Delay_Req, Signaling, Management, Pdelay_Req, 
* Pdelay_Resp, Pdelay_Resp_Follow_Up)
*/
#define PTP_MSG_DEFAULT_INTERVAL    0x7F        ///< default interval

/**
* PTP header format (common header for all PTP messages).
*/
struct ptp_header {
    u8 msg_type;                ///< ptp message type (messageType) (bits 3-0), transportSpecific (bits 7-4)
    u8 ptp_ver;                 ///< ptp version (bits 3-0)(versionPTP)
    u16 msg_len;                ///< messageLength
    u8 domain_num;              ///< domainNumber
    u8 res;
    u16 flags;                  ///< flags
    u64 corr_field;             ///< correctionField
    u32 res1;
    struct PortIdentity src_port_id;    ///< sourcePortIdentity 
    u16 seq_id;                 ///< sequenceId
    u8 control;                 ///< control 
    u8 log_mean_msg_interval;   ///< logMeanMessageInterval
} __attribute__ ((packed));


/**
* PTP announce message.
*/
struct ptp_announce {
    struct ptp_header hdr;      ///< common PTP header
    u8 origin_tstamp[10];       ///< originTimestamp 
    u16 current_UTC_offset;     ///< currentUTCOffset
    u8 res;
    u8 grandmasterPri1;         ///< grandmasterPriority1
    struct ClockQuality grandmasterClkQuality;  ///< grandmasterClockQuality
    u8 grandmasterPri2;         ///< grandmasterPriority2    
    ClockIdentity grandmasterId;        ///< grandmasterIdentity
    u16 steps_removed;          ///< stepsRemoved
    u8 time_source;             ///< timeSource
} __attribute__ ((packed));

/**
* PTP Sync message.
*/
struct ptp_sync {
    struct ptp_header hdr;      ///< common PTP header
    u8 origin_tstamp[10];       ///< originTimestamp 
} __attribute__ ((packed));

/**
* PTP Delay_Req message.
*/
struct ptp_delay_req {
    struct ptp_header hdr;      ///< common PTP header
    u8 origin_tstamp[10];       ///< originTimestamp 
} __attribute__ ((packed));

/**
* PTP Follw_Up message.
*/
struct ptp_follow_up {
    struct ptp_header hdr;      ///< common PTP header
    u8 precise_origin_tstamp[10];       ///< preciseOriginTimestamp 
} __attribute__ ((packed));

/**
* PTP Delay_Resp message.
*/
struct ptp_delay_resp {
    struct ptp_header hdr;      ///< common PTP header
    u8 recv_tstamp[10];         ///< receiveTimestamp 
    struct PortIdentity req_port_id;    ///< requestingPortIdentity 
} __attribute__ ((packed));

/**
* PTP Pdelay_Req message.
*/
struct ptp_pdelay_req {
    struct ptp_header hdr;      ///< common PTP header
    u8 origin_tstamp[10];       ///< originTimestamp 
    u8 res[10];
} __attribute__ ((packed));

/**
* PTP Pdelay_Resp message.
*/
struct ptp_pdelay_resp {
    struct ptp_header hdr;      ///< common PTP header
    u8 recv_tstamp[10];         ///< requestReceiptTimestamp 
    u8 req_port_id[10];         ///< requestingPortIdentity 
} __attribute__ ((packed));

/**
* PTP Pdelay_Resp_Follow_Up message.
*/
struct ptp_pdelay_resp_follow_up {
    struct ptp_header hdr;      ///< common PTP header
    u8 resp_origin_tstamp[10];  ///< responseOriginTimestamp 
    u8 req_port_id[10];         ///< requestingPortIdentity 
} __attribute__ ((packed));

/**
* PTP Signaling message.
*/
struct ptp_signaling {
    struct ptp_header hdr;      ///< common PTP header
    u8 target_port_id[10];      ///< targetPortIdentity
    u8 tlvs;                    ///< One or more TLVs
} __attribute__ ((packed));

#endif                          // _PTP_MESSAGE_H_
