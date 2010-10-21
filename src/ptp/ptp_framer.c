/** @file ptp_framer.c
* Create PTP messages.
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
#include "ptp.h"
#include "ptp_framer.h"
#include "ptp_general.h"
#include "ptp_internal.h"
#include "ptp_message.h"
#include "ptp_port.h"

/**
* Function for creating PTP sync message.
* @param ctx Port context.
* @param buf Buffer to which frame is created.
* @param seqid Sequence id.
* @return size of the created frame.
*/
int create_sync(struct ptp_port_ctx *ctx, char *buf, u16 seqid)
{
    struct ptp_sync *msg = 0;
    unsigned short len = sizeof(struct ptp_sync);
    struct Timestamp time;
    int ret = 0;

    DEBUG("\n");

    memset(buf, 0, len);

    // Header fields
    msg = (struct ptp_sync *) buf;
    msg->hdr.msg_type = PTP_SYNC;       // ptp message type 
    msg->hdr.ptp_ver = ctx->port_dataset.version_number;        // ptp version
    msg->hdr.msg_len = htons(len);      // messageLength
    msg->hdr.domain_num = ptp_ctx.default_dataset.domain;       // domainNumber
    // flags
    if (ptp_cfg.one_step_clock == 1) {
        msg->hdr.flags = 0;
    } else {
        msg->hdr.flags = PTP_TWO_STEP;
    }
    if (ctx->unicast_port) {
        msg->hdr.flags |= PTP_UNICAST;
    }
    msg->hdr.corr_field = 0;    // correctionField
    memcpy(msg->hdr.src_port_id.clock_identity, ctx->port_dataset.port_identity.clock_identity, sizeof(ClockIdentity)); // sourcePortIdentity 
    msg->hdr.src_port_id.port_number =
        htons(ctx->port_dataset.port_identity.port_number);
    msg->hdr.seq_id = htons(seqid);
    msg->hdr.control = PTP_CTRL_SYNC;   // control 
    // logMeanMessageInterval
    msg->hdr.log_mean_msg_interval =
        ctx->port_dataset.log_mean_sync_interval;

    // Sync msg content (timestamp)
    if (ptp_cfg.one_step_clock == 1) {
        memset(&time, 0, sizeof(struct Timestamp));
    } else {
        ret = ptp_get_time(&ptp_ctx.clk_ctx, &time);
        if (ret < PTP_ERR_OK) {
            return ret;
        }
    }
    ptp_format_timestamp(&time, msg->origin_tstamp);    ///< originTimestamp 

    return len;
}

/**
* Function for creating PTP Follow_Up message.
* @param ctx Port context.
* @param buf Buffer to which frame is created.
* @param time when sync was sent
* @param seqid Sequence id.
* @return size of the created frame.
*/
int create_follow_up(struct ptp_port_ctx *ctx,
                     char *buf, struct Timestamp *time, u16 seqid)
{
    struct ptp_follow_up *msg = 0;
    unsigned short len = sizeof(struct ptp_follow_up);

    DEBUG("\n");

    memset(buf, 0, len);

    // Header fields
    msg = (struct ptp_follow_up *) buf;
    msg->hdr.msg_type = PTP_FOLLOW_UP;  // ptp message type
    msg->hdr.ptp_ver = ctx->port_dataset.version_number;        // version
    msg->hdr.msg_len = htons(len);      // messageLength
    msg->hdr.domain_num = ptp_ctx.default_dataset.domain;       // domainNumber
    // flags
    if (ptp_cfg.one_step_clock == 1) {
        msg->hdr.flags = 0;
    } else {
        msg->hdr.flags = PTP_TWO_STEP;
    }
    if (ctx->unicast_port) {
        msg->hdr.flags |= PTP_UNICAST;
    }
    msg->hdr.corr_field = 0;    ///< correctionField
    memcpy(msg->hdr.src_port_id.clock_identity, ctx->port_dataset.port_identity.clock_identity, sizeof(ClockIdentity)); // sourcePortIdentity 
    msg->hdr.src_port_id.port_number =
        htons(ctx->port_dataset.port_identity.port_number);
    msg->hdr.seq_id = htons(seqid);     // sequenceId 
    msg->hdr.control = PTP_CTRL_FOLLOW_UP;      // control 
    // logMeanMessageInterval
    msg->hdr.log_mean_msg_interval =
        ctx->port_dataset.log_mean_sync_interval;

    DEBUG("%is %i\n", (int) time->seconds, (int) time->nanoseconds);
    // Follow up msg content (timestamp)
    ptp_format_timestamp(time, msg->precise_origin_tstamp);

    return len;
}

/**
* Function for creating PTP Announce message.
* @param ctx Port context.
* @param buf Buffer to which frame is created.
* @param seqid Sequence id.
* @param local if true, use data from defaut dataset (instead of parent).
* @return size of the created frame.
*/
int create_announce(struct ptp_port_ctx *ctx, char *buf, 
                    u16 seqid, int local)
{
    struct ptp_announce *msg = 0;
    unsigned short len = sizeof(struct ptp_announce);
    struct Timestamp time;
    int ret = 0;

    DEBUG("\n");

    memset(buf, 0, len);

    // Header fields
    msg = (struct ptp_announce *) buf;
    msg->hdr.msg_type = PTP_ANNOUNCE;   // ptp message type 
    msg->hdr.ptp_ver = ctx->port_dataset.version_number;        // version 
    msg->hdr.msg_len = htons(len);      // messageLength
    msg->hdr.domain_num = ptp_ctx.default_dataset.domain;       // domainNumber
    // flags
    msg->hdr.flags =
        ptp_ctx.time_dataset.time_traceable ? PTP_TIME_TRACEABLE : 0 |
        ptp_ctx.time_dataset.frequency_traceable ? PTP_FREQ_TRACEABLE : 0 |
        ptp_ctx.time_dataset.ptp_timescale ? PTP_TIMESCALE : 0 |
        ptp_ctx.time_dataset.
        current_utc_offset_valid ? PTP_UTC_OFFSET_VALID : 0;
    if (ctx->unicast_port) {
        msg->hdr.flags |= PTP_UNICAST;
    }
    msg->hdr.corr_field = 0;    // correctionField
    memcpy(msg->hdr.src_port_id.clock_identity, ctx->port_dataset.port_identity.clock_identity, sizeof(ClockIdentity)); // sourcePortIdentity 
    msg->hdr.src_port_id.port_number =
        htons(ctx->port_dataset.port_identity.port_number);
    msg->hdr.seq_id = htons(seqid);
    msg->hdr.control = PTP_CTRL_OTHER;  // control 
    // logMeanMessageInterval
    msg->hdr.log_mean_msg_interval =
        ctx->port_dataset.log_mean_announce_interval;

    // Announce msg content
    // Timestamp
    ret = ptp_get_time(&ptp_ctx.clk_ctx, &time);
    if (ret < PTP_ERR_OK) {
        return ret;
    }
    ptp_format_timestamp(&time, msg->origin_tstamp);    // originTimestamp
    msg->current_UTC_offset = htons(ptp_ctx.time_dataset.current_utc_offset);   // currentUTCOffset
    msg->time_source = ptp_ctx.time_dataset.time_source;        // timeSource 
    // stepsRemoved
    msg->steps_removed = htons(ptp_ctx.current_dataset.steps_removed);
    if( local ){
        memcpy(msg->grandmasterId, 
               ptp_ctx.default_dataset.clock_identity, 
               sizeof(ClockIdentity));     // grandmasterIdentity
        msg->grandmasterClkQuality.clock_class =
            ptp_ctx.default_dataset.clock_quality.clock_class;
        msg->grandmasterClkQuality.clock_accuracy =
            ptp_ctx.default_dataset.clock_quality.clock_accuracy;
        msg->grandmasterClkQuality.offset_scaled_log_variance =
            htons(ptp_ctx.default_dataset.clock_quality.
                  offset_scaled_log_variance);
        // GM priority1
        msg->grandmasterPri1 = ptp_ctx.default_dataset.priority1;  
        // GM priority2  
        msg->grandmasterPri2 = ptp_ctx.default_dataset.priority2;  
    }
    else {
        memcpy(msg->grandmasterId, 
               ptp_ctx.parent_dataset.grandmaster_identity, 
               sizeof(ClockIdentity));     // grandmasterIdentity
        msg->grandmasterClkQuality.clock_class =
            ptp_ctx.parent_dataset.grandmaster_clock_quality.clock_class;
        msg->grandmasterClkQuality.clock_accuracy =
            ptp_ctx.parent_dataset.grandmaster_clock_quality.clock_accuracy;
        msg->grandmasterClkQuality.offset_scaled_log_variance =
            htons(ptp_ctx.parent_dataset.grandmaster_clock_quality.
                  offset_scaled_log_variance);
        // GM priority1
        msg->grandmasterPri1 = ptp_ctx.parent_dataset.grandmaster_priority1;  
        // GM priority2  
        msg->grandmasterPri2 = ptp_ctx.parent_dataset.grandmaster_priority2;  
    }
    return len;
}

/**
* Function for creating PTP Delay_Req message.
* @param ctx Port context.
* @param buf Buffer to which frame is created.
* @param seqid Sequence id.
* @return size of the created frame.
*/
int create_delay_req(struct ptp_port_ctx *ctx, char *buf, u16 seqid)
{
    struct ptp_delay_req *msg = 0;
    unsigned short len = sizeof(struct ptp_delay_req);
    struct Timestamp time;
    int64_t delay_asymmetry = 0;
    int ret = 0;

    DEBUG("\n");

    memset(buf, 0, len);

    // Header fields
    msg = (struct ptp_delay_req *) buf;
    msg->hdr.msg_type = PTP_DELAY_REQ;  // ptp message type 
    msg->hdr.ptp_ver = ctx->port_dataset.version_number;        // version
    msg->hdr.msg_len = htons(len);      // messageLength
    msg->hdr.domain_num = ptp_ctx.default_dataset.domain;       // domainNumber
    // flags
    msg->hdr.flags = 0;
    if (ctx->unicast_port) {
        msg->hdr.flags |= PTP_UNICAST;
    }
    if( ctx->delay_asymmetry_master_set ){
        if( !memcmp(ctx->delay_asymmetry_master,
                    ctx->current_master,
                    sizeof(ClockIdentity))){
            delay_asymmetry = ctx->delay_asymmetry;
        }
    }
    else {
        delay_asymmetry = ctx->delay_asymmetry;
    }
    // Scale delay asymmetry from ps to ns.sns 
    delay_asymmetry = (delay_asymmetry << 16)/1000;
        
    // correctionField, remove asymmetry
    msg->hdr.corr_field = htonll( -delay_asymmetry );    

    memcpy(msg->hdr.src_port_id.clock_identity, 
           ctx->port_dataset.port_identity.clock_identity, 
           sizeof(ClockIdentity)); // sourcePortIdentity 
    msg->hdr.src_port_id.port_number =
        htons(ctx->port_dataset.port_identity.port_number);
    msg->hdr.seq_id = htons(seqid);
    msg->hdr.control = PTP_CTRL_DELAY_REQ;      // control 
    msg->hdr.log_mean_msg_interval = 0x7f;      // logMeanMessageInterval

    // Delay_req msg content (timestamp)
    ret = ptp_get_time(&ptp_ctx.clk_ctx, &time);
    if (ret < PTP_ERR_OK) {
        ERROR("ptp_get_time\n");
        return ret;
    }
    ptp_format_timestamp(&time, msg->origin_tstamp);

    DEBUG("%i\n", len);

    return len;
}

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
int create_delay_resp(struct ptp_port_ctx *ctx,
                      char *buf,
                      struct Timestamp *time,
                      struct PortIdentity *src_port_id,
                      u16 seqid, u64 corr_field)
{
    struct ptp_delay_resp *msg = 0;
    unsigned short len = sizeof(struct ptp_delay_resp);

    DEBUG("\n");

    memset(buf, 0, len);

    // Header fields
    msg = (struct ptp_delay_resp *) buf;
    msg->hdr.msg_type = PTP_DELAY_RESP; // ptp message type 
    msg->hdr.ptp_ver = ctx->port_dataset.version_number;        // version
    msg->hdr.msg_len = htons(len);      // messageLength
    msg->hdr.domain_num = ptp_ctx.default_dataset.domain;       // domainNumber
    // flags
    msg->hdr.flags = 0;
    if (ctx->unicast_port) {
        msg->hdr.flags |= PTP_UNICAST;
    }
    msg->hdr.corr_field = htonll(corr_field);   // correction field
    memcpy(msg->hdr.src_port_id.clock_identity, ctx->port_dataset.port_identity.clock_identity, sizeof(ClockIdentity)); // sourcePortIdentity 
    msg->hdr.src_port_id.port_number =
        htons(ctx->port_dataset.port_identity.port_number);
    msg->hdr.seq_id = htons(seqid);     // sequenceId 
    msg->hdr.control = PTP_CTRL_DELAY_RESP;     // control
    // logMeanMessageInterval
    msg->hdr.log_mean_msg_interval =
        ctx->port_dataset.log_min_mean_delay_req_interval;

    // Delay_resp msg content 
    // Timestamp
    ptp_format_timestamp(time, msg->recv_tstamp);       // originTimestamp 
    // Copy delay_req src port id
    memcpy(msg->req_port_id.clock_identity,
           src_port_id->clock_identity, sizeof(ClockIdentity));
    msg->req_port_id.port_number = htons(src_port_id->port_number);

    return len;

}
