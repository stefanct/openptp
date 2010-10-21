/** @file ptp_port_packet.c
* PTP port specific functions.
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
#include "ptp_internal.h"
#include "ptp_message.h"
#include "ptp_port.h"
#include "ptp_framer.h"

/**
* Function for reporting new PTP port. After completion of this function call,
* PTP module may start sending and receiving to this port.
* @see port_id 
* @param port_num port number.
* @param identity clock identity.
* @param unicast_port Flag to indicate that port is using unicast.
* @param if_config Interface configuration.
*/
void ptp_new_port(int port_num, 
                  ClockIdentity identity, 
                  bool unicast_port,
                  struct interface_config* if_config )
{
    struct ptp_port_ctx *ctx = ptp_ctx.ports_list_head;

    // Check that this port id is not in use
    while (ctx != NULL) {
        if (ctx->port_dataset.port_identity.port_number == port_num) {
            // match found
            ERROR("port_num is in use!!!\n");
            return;
        }
        ctx = ctx->next;
    }

    ctx = malloc(sizeof(struct ptp_port_ctx));
    if (ctx == 0) {
        ERROR("Allocation of new port ctx failed\n");
        return;
    }
    memset(ctx, 0, sizeof(struct ptp_port_ctx));
    ctx->unicast_port = unicast_port;
    ctx->delay_asymmetry = if_config->delay_asymmetry;
    if( if_config->delay_asymmetry_master_set ){
        ctx->delay_asymmetry_master_set = 1;
        memcpy(ctx->delay_asymmetry_master,
               if_config->delay_asymmetry_master,
               sizeof(ClockIdentity));
    }
    strncpy(ctx->name, if_config->name, INTERFACE_NAME_LEN);

    // Init portdataset
    memcpy(ctx->port_dataset.port_identity.clock_identity,
           identity, sizeof(ClockIdentity));
    ctx->port_dataset.port_identity.port_number = port_num;

    // Init data from config file
    init_port_dataset( &ctx->port_dataset );

    ctx->port_dataset.version_number = PTP_VERSION;
    ctx->port_dataset.announce_receipt_timeout = ANNOUNCE_WINDOW;
    ctx->port_dataset.delay_mechanism = DELAY_DISABLED;
    
    ptp_port_state_update(ctx, PORT_INITIALIZING);

    /* Update default dataset (not needed for every port registration, 
     * because this is static) */
    if (port_num == 1) {
        // Set first port identity as clock identity
        memcpy(ptp_ctx.default_dataset.clock_identity, 
               identity, sizeof(ClockIdentity));// Local clock identity
    }
    ptp_ctx.default_dataset.num_ports++;

    // Add to port list
    ctx->next = ptp_ctx.ports_list_head;
    ptp_ctx.ports_list_head = ctx;

    DEBUG("Added port %i/%i %p %s\n", 
          port_num, ptp_ctx.default_dataset.num_ports, 
          ctx, ptp_clk_id(identity));
}

/**
* Function for closing existing PTP port. After completion of this function 
* call, PTP module must stop sending and receiving to this port. All 
* pending send operations shall be completed with frame_sent.
* @see port_num 
* @param port_num port number.
*/
void ptp_close_port(int port_num)
{
    struct ptp_port_ctx *tmp_ctx = ptp_ctx.ports_list_head, *prev_ctx = 0;
    DEBUG("\n");

    // Remove from queue
    while (tmp_ctx != NULL) {
        if (tmp_ctx->port_dataset.port_identity.port_number == port_num) {
            // match found
            if (!prev_ctx) {
                ptp_ctx.ports_list_head = tmp_ctx->next;
            } else {
                prev_ctx = tmp_ctx->next;
            }
            break;
        }
        tmp_ctx = tmp_ctx->next;
    }
    if (!tmp_ctx) {
        ERROR("NOT FOUND\n");
    } else {
        free(tmp_ctx);
        // Update default dataset
        ptp_ctx.default_dataset.num_ports--;
        DEBUG("Closed port %i\n", port_num);
    }
}

/**
* Function for reporting the completion of sending of the PTP event 
* frame. Called for Delay_req and Sync frames (if two step clock). 
* @see send.
* @param port_num port number.
* @param msg_hdr Header of the sent frame.
* @param error sending process failed, ptp error number returned.
* @param sent_time timestamp of the sent ptp frame.
*/
void ptp_frame_sent(int port_num,
                    struct ptp_header *msg_hdr,
                    int error, struct Timestamp *sent_time)
{
    struct ptp_port_ctx *ctx = ptp_ctx.ports_list_head;

    while (ctx != NULL) {
        if (ctx->port_dataset.port_identity.port_number == port_num) {
            // match found
            break;
        }
        ctx = ctx->next;
    }
    if (!ctx) {
        ERROR("Port not found\n");
        return;
    }

    switch (msg_hdr->msg_type & 0x0f) {
    case PTP_SYNC:
        if (ptp_cfg.one_step_clock == 0) {
            // This is executed only if TWO_STEP_CLOCK==1
            char tmpbuf[MAX_PTP_FRAME_SIZE];
            int ret = 0;
            DEBUG("Sync sent, send follow_up\n");
            // create follow_up
            ret = create_follow_up(ctx, tmpbuf, sent_time,
                                   htons(msg_hdr->seq_id));
            if (ret > 0) {
                DEBUG("Send Follow up\n");
                ret = ptp_send(&ptp_ctx.pkt_ctx, PTP_FOLLOW_UP,
                               port_num, tmpbuf, ret);
                if( ret != PTP_ERR_OK ){
                    socket_restart = 1;
                }
            }
        }
        break;
    case PTP_DELAY_REQ:
        DEBUG("delay_req sent, store timestamp\n");
        // Store timestamp and seq_id
        ctx->delay_req_seqid_sent = ntohs(msg_hdr->seq_id);
        copy_timestamp(&ctx->delay_req_send_time, sent_time);
        break;
    default:
        // Nothing
        break;
    }
}
