/** @file ptp_port_recv.c
* PTP port receive handlers.
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
#include "ptp_general.h"
#include "ptp_message.h"
#include "ptp_port.h"
#include "ptp_framer.h"
#include "ptp_internal.h"

// Functions for handling specific PTP frames
static void ptp_port_recv_sync(struct ptp_port_ctx *ctx,
                               struct ptp_sync *msg,
                               struct Timestamp *time);
static void ptp_port_recv_follow_up(struct ptp_port_ctx *ctx,
                                    struct ptp_follow_up *msg,
                                    struct Timestamp *time);
static void ptp_port_recv_announce(struct ptp_port_ctx *ctx,
                                   struct ptp_announce *msg,
                                   struct Timestamp *time);
static void ptp_port_recv_delay_req(struct ptp_port_ctx *ctx,
                                    struct ptp_delay_req *msg,
                                    struct Timestamp *time);
static void ptp_port_recv_delay_resp(struct ptp_port_ctx *ctx,
                                     struct ptp_delay_resp *msg,
                                     struct Timestamp *time);

/**
* Function for handling all received PTP messages for port.
* @param ctx Port context.
* @param buf PTP message.
* @param len msg length.
* @param time timestamp for received frame.
*/
void ptp_port_recv(struct ptp_port_ctx *ctx,
                   char *buf, int len, struct Timestamp *time)
{
    struct ptp_header *hdr = (struct ptp_header *) buf;

    if (hdr->domain_num != ptp_ctx.default_dataset.domain) {
        DEBUG("PTP message from wrong domain\n");
        return;
    }

    switch (hdr->msg_type & 0x0f) {
    case PTP_SYNC:
        DEBUG("PTP_SYNC 0x%012llxs 0x%08x.%04xns(0x%llx): %i\n",
              time->seconds,
              time->nanoseconds,
              time->frac_nanoseconds,
              ntohll(hdr->corr_field), ntohs(hdr->seq_id));
        ptp_port_recv_sync(ctx, (struct ptp_sync *) hdr, time);
        break;
    case PTP_FOLLOW_UP:
        DEBUG("PTP_FOLLOW_UP 0x%012llxs 0x%08x.%04xns(0x%llx): %i\n",
              time->seconds,
              time->nanoseconds,
              time->frac_nanoseconds,
              ntohll(hdr->corr_field), ntohs(hdr->seq_id));
        ptp_port_recv_follow_up(ctx, (struct ptp_follow_up *) hdr, time);
        break;
    case PTP_DELAY_REQ:
        DEBUG("PTP_DELAY_REQ 0x%012llxs 0x%08x.%04xns(0x%llx): %i\n",
              time->seconds,
              time->nanoseconds,
              time->frac_nanoseconds,
              ntohll(hdr->corr_field), ntohs(hdr->seq_id));
        ptp_port_recv_delay_req(ctx, (struct ptp_delay_req *) hdr, time);
        break;
    case PTP_ANNOUNCE:
        DEBUG("PTP_ANNOUNCE  0x%012llxs 0x%08x.%04xns(0x%llx): %i\n",
              time->seconds,
              time->nanoseconds,
              time->frac_nanoseconds,
              ntohll(hdr->corr_field), ntohs(hdr->seq_id));
        ptp_port_recv_announce(ctx, (struct ptp_announce *) hdr, time);
        break;
    case PTP_DELAY_RESP:
        DEBUG("PTP_DELAY_RESP 0x%012llxs 0x%08x.%04xns(0x%llx): %i\n",
              time->seconds,
              time->nanoseconds,
              time->frac_nanoseconds,
              ntohll(hdr->corr_field), ntohs(hdr->seq_id));
        ptp_port_recv_delay_resp(ctx, (struct ptp_delay_resp *) hdr, time);
        break;
    case PTP_PDELAY_REQ:
    case PTP_PDELAY_RESP:
    case PTP_PDELAY_RESP_FOLLOW_UP:
    case PTP_SIGNALING:
    case PTP_MANAGEMENT:
        DEBUG("TODO: %i\n", hdr->msg_type & 0x0f);
        break;
    }

}

/**
* Function for handling received Sync.
* @param ctx Port context.
* @param msg Sync message.
* @param time frame timestamp
*/
static void ptp_port_recv_sync(struct ptp_port_ctx *ctx,
                               struct ptp_sync *msg,
                               struct Timestamp *time)
{
    DEBUG("\n");

    // Check port state
    if ((ctx->port_dataset.port_state == PORT_INITIALIZING) ||
        (ctx->port_dataset.port_state == PORT_DISABLED) ||
        (ctx->port_dataset.port_state == PORT_FAULTY)) {
        // Discard
        return;
    }
    if ((ctx->port_dataset.port_state == PORT_SLAVE) ||
        (ctx->port_dataset.port_state == PORT_UNCALIBRATED)) {
        // Check sync
        // Check if message is from current master
        if (memcmp(ctx->current_master,
                   msg->hdr.src_port_id.clock_identity,
                   sizeof(ClockIdentity)) == 0) {
            DEBUG("Sync from current master %i\n", ntohs(msg->hdr.seq_id));
            if (!(msg->hdr.flags & PTP_TWO_STEP)) {
                // ONE_STEP master->sync local clk
                struct Timestamp master_time;
                ptp_convert_timestamp(&master_time, msg->origin_tstamp);
                add_correction(&master_time,
                               ntohll(msg->hdr.corr_field) +
                               ASYMMETRY_CORRECTION);
                ptp_sync_rcv(&ptp_ctx.clk_ctx, &master_time, time);
            } else {            // Store seq_id and timestamp
                ctx->sync_seqid = ntohs(msg->hdr.seq_id);
                ctx->sync_recv_corr_field = ntohll(msg->hdr.corr_field);
                copy_timestamp(&ctx->sync_recv_time, time);
            }
        }
    }
}

/**
* Function for handling received Follow_Up.
* @param ctx Port context.
* @param msg Sync message.
* @param time frame timestamp
*/
static void ptp_port_recv_follow_up(struct ptp_port_ctx *ctx,
                                    struct ptp_follow_up *msg,
                                    struct Timestamp *time)
{

    DEBUG("\n");

    // Check port state
    if ((ctx->port_dataset.port_state == PORT_INITIALIZING) ||
        (ctx->port_dataset.port_state == PORT_DISABLED) ||
        (ctx->port_dataset.port_state == PORT_FAULTY)) {
        // Discard
        return;
    }
    if ((ctx->port_dataset.port_state == PORT_SLAVE) ||
        (ctx->port_dataset.port_state == PORT_UNCALIBRATED)) {
        // Check sync
        // Check if message is from current master
        if (memcmp(ctx->current_master,
                   msg->hdr.src_port_id.clock_identity,
                   sizeof(ClockIdentity)) == 0) {
            // Associate to recvd sync
            if (ctx->sync_seqid == ntohs(msg->hdr.seq_id)) {
                struct Timestamp master_time;
                DEBUG("Follow_up from current master %i\n",
                      ntohs(msg->hdr.seq_id));
                // calc 
                ptp_convert_timestamp(&master_time,
                                      msg->precise_origin_tstamp);
                add_correction(&master_time, ctx->sync_recv_corr_field +
                               ntohll(msg->hdr.corr_field));
                ptp_sync_rcv(&ptp_ctx.clk_ctx,
                             &master_time, &ctx->sync_recv_time);
            } else {
                ERROR
                    ("Follow_up from current master, seq_id mismatch:%i %i\n",
                     ctx->sync_seqid, ntohs(msg->hdr.seq_id));
            }
        }
    }
}

/**
* Function for handling received Announce.
* @param ctx Port context.
* @param msg Sync message.
* @param time frame timestamp
*/
static void ptp_port_recv_announce(struct ptp_port_ctx *ctx,
                                   struct ptp_announce *msg,
                                   struct Timestamp *time)
{
    struct ForeignMasterDataSetElem *foreign_elem =
        ctx->foreign_master_head;
    struct ForeignMasterDataSet *foreign = 0;
    int i = 0, num_foreign_masters = 0;

    DEBUG("\n");

    // Check port state
    if ((ctx->port_dataset.port_state == PORT_INITIALIZING) ||
        (ctx->port_dataset.port_state == PORT_DISABLED) ||
        (ctx->port_dataset.port_state == PORT_FAULTY)) {
        // Discard
        return;
    }
    // Qualify announce message
    // Check ALTERNATE_MASTER flag
    if (msg->hdr.flags & PTP_ALTERNATE_MASTER) {
        // Announce with ALTERNATE_MASTER flag not accepted
        return;
    }

    if ((ctx->port_dataset.port_state == PORT_UNCALIBRATED) ||
        (ctx->port_dataset.port_state == PORT_SLAVE)) {
        if (((memcmp(msg->hdr.src_port_id.clock_identity,
                     ptp_ctx.parent_dataset.parent_port_identity.
                     clock_identity, sizeof(ClockIdentity))) == 0)
            && (ntohs(msg->hdr.src_port_id.port_number) ==
                ptp_ctx.parent_dataset.parent_port_identity.port_number)) {
            // match
            ptp_port_announce_recv_timeout_restart(ctx, time);
        }
    }

    if (ctx->port_dataset.port_state == PORT_PASSIVE) {
        if (memcmp(msg->hdr.src_port_id.clock_identity,
                   ctx->current_master, sizeof(ClockIdentity)) == 0) {
            // match
            ptp_port_announce_recv_timeout_restart(ctx, time);
        }
    }

    while (foreign_elem) {
        foreign = foreign_elem->data_p;
        num_foreign_masters++;
        // check if from a known foreign master (port)
        if (((memcmp(msg->hdr.src_port_id.clock_identity,
                     foreign->src_port_id.clock_identity,
                     sizeof(ClockIdentity))) == 0) &&
            (ntohs(msg->hdr.src_port_id.port_number) ==
             foreign->src_port_id.port_number)) {
            // found, continue 
            break;
        }
        foreign = 0;
        foreign_elem = foreign_elem->next;
    }

    if (foreign) {              // found
        DEBUG("Update foreign record ");
        for (i = 0; i < 9; i++) {
            DEBUG_PLAIN("%02x:",
                        0xff & ((char *) &foreign->src_port_id)[i]);
        }
        DEBUG_PLAIN("%02x\n", 0xff & ((char *) &foreign->src_port_id)[i]);

        memcpy(&foreign->announce_tstamp[foreign->tstamp_index],
               time, sizeof(struct Timestamp));
        foreign->tstamp_index++;
        foreign->tstamp_index %= ANNOUNCE_WINDOW;
        memcpy(&foreign->msg, msg, sizeof(struct ptp_announce));
    } else if (num_foreign_masters >= MAX_NUM_FOREIGN_MASTERS) {
        DEBUG("List of foreign masters full\n");
    } else {                    // unknown foreign, and room in list
        // Create new foreign record
        foreign_elem = malloc(sizeof(struct ForeignMasterDataSetElem));
        if (!foreign_elem) {
            ERROR("Mem alloc fail\n");
        } else {
            memset(foreign_elem, 0,
                   sizeof(struct ForeignMasterDataSetElem));
            foreign_elem->data_p = &foreign_elem->data;
            foreign = foreign_elem->data_p;

            // copy source clock id ...
            memcpy(foreign->src_port_id.clock_identity,
                   msg->hdr.src_port_id.clock_identity,
                   sizeof(ClockIdentity));
            // ... and port
            foreign->src_port_id.port_number =
                ntohs(msg->hdr.src_port_id.port_number);
            // copy destination clock id ...
            memcpy(foreign->dst_port_id.clock_identity,
                   ctx->port_dataset.port_identity.clock_identity,
                   sizeof(ClockIdentity));
            // ... and port
            foreign->dst_port_id.port_number =
                ctx->port_dataset.port_identity.port_number;

            // Update timestamps
            foreign->foreign_master_announce_messages = 1;
            memcpy(&foreign->announce_tstamp[foreign->tstamp_index],
                   time, sizeof(struct Timestamp));
            foreign->tstamp_index++;
            // Store announce for BMC use
            memcpy(&foreign->msg, msg, sizeof(struct ptp_announce));
            // Add foreign to list  
            foreign_elem->next = ctx->foreign_master_head;
            ctx->foreign_master_head = foreign_elem;

            DEBUG("Added foreign record ");
            for (i = 0; i < 8; i++) {
                DEBUG_PLAIN("%02x:",
                            0xff & foreign->src_port_id.clock_identity[i]);
            }
            DEBUG_PLAIN("%x\n", 0xffff & foreign->src_port_id.port_number);
        }
    }
}

/**
* Function for handling received Delay_req.
* @param ctx Port context.
* @param msg Delay_req message.
* @param time frame timestamp
*/
static void ptp_port_recv_delay_req(struct ptp_port_ctx *ctx,
                                    struct ptp_delay_req *msg,
                                    struct Timestamp *time)
{
    char tmpbuf[MAX_PTP_FRAME_SIZE];
    int ret = 0;
    struct PortIdentity port_id;

    DEBUG("\n");
    // Check port state
    if ((ctx->port_dataset.port_state == PORT_INITIALIZING) ||
        (ctx->port_dataset.port_state == PORT_DISABLED) ||
        (ctx->port_dataset.port_state == PORT_FAULTY)) {
        // Discard
        return;
    }

    if (ctx->port_dataset.port_state == PORT_MASTER) {
        // Get port id
        memcpy(port_id.clock_identity,
               msg->hdr.src_port_id.clock_identity, sizeof(ClockIdentity));
        port_id.port_number = ntohs(msg->hdr.src_port_id.port_number);

        add_correction(time, ASYMMETRY_CORRECTION);

        // create delay_resp
        ret = create_delay_resp(ctx, tmpbuf, time, &port_id,
                                htons(msg->hdr.seq_id),
                                ntohll(msg->hdr.corr_field));
        if (ret > 0) {
            DEBUG("Send Delay_resp\n");
            ret = ptp_send(&ptp_ctx.pkt_ctx, PTP_DELAY_RESP,
                           ctx->port_dataset.port_identity.port_number,
                           tmpbuf, ret);
        }
    }
}

/**
* Function for handling received Delay_Resp.
* @param ctx Port context.
* @param msg Delay_req message.
* @param time frame timestamp
*/
static void ptp_port_recv_delay_resp(struct ptp_port_ctx *ctx,
                                     struct ptp_delay_resp *msg,
                                     struct Timestamp *time)
{
    DEBUG("\n");

    // Check port state
    if ((ctx->port_dataset.port_state == PORT_INITIALIZING) ||
        (ctx->port_dataset.port_state == PORT_DISABLED) ||
        (ctx->port_dataset.port_state == PORT_FAULTY)) {
        // Discard
        return;
    }

    if ((ctx->port_dataset.port_state == PORT_SLAVE) ||
        (ctx->port_dataset.port_state == PORT_UNCALIBRATED)) {
        // Check Delay_Resp
        // sequence_id and current_master
        if (((ctx->delay_req_seqid - 1) == ntohs(msg->hdr.seq_id)) &&
            (memcmp(ctx->current_master,
                    msg->hdr.src_port_id.clock_identity,
                    sizeof(ClockIdentity)) == 0)) {
            struct Timestamp master_time;
            DEBUG("Delay_resp from current master\n");
            ptp_convert_timestamp(&master_time, msg->recv_tstamp);
            add_correction(&ctx->delay_req_send_time,
                           ntohll(msg->hdr.corr_field) -
                           ASYMMETRY_CORRECTION);
            ptp_delay_rcv(&ptp_ctx.clk_ctx, &ctx->delay_req_send_time,
                          &master_time);
        } else if ((ctx->delay_req_seqid - 1) != ntohs(msg->hdr.seq_id)) {
            DEBUG("delay_req seq_id mismatch %i %i\n",
                  (ctx->delay_req_seqid - 1), ntohs(msg->hdr.seq_id));
        }
    }
}
