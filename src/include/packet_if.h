/** @file packet_if.h
* Packet interface description.
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

/*******************************************************************************
* $Id$
*******************************************************************************/

#ifndef _PACKET_IF_H_
#define _PACKET_IF_H_

#include "ptp_general.h"
#include "ptp_message.h"

/**
* Packet interface context information.
*/
struct packet_ctx {
    void *arg;                  ///< private data for os/if specific data    
};

/** These API functions are called by PTP module and implemented by 
* packet module. 
*/

/**
* Function for initializing packet interface. 
* @param ctx packet if context
* @param cfg_file Config file name for packet interface.
* @return ptp error code.
*/
int ptp_initialize_packet_if(struct packet_ctx *ctx, char* cfg_file);

/**
* Function for reconfiguring packet interface. 
* @param ctx packet if context
* @param cfg_file Config file name for packet interface.
* @return ptp error code.
*/
int ptp_reconfig_packet_if(struct packet_ctx *ctx, char* cfg_file);

/**
* Function for closing packet interface. 
* @param ctx packet if context
* @return ptp error code.
*/
int ptp_close_packet_if(struct packet_ctx *ctx);

/**
* Function for sending PTP frames. 
* @see frame_sent.
* @param ctx packet if context
* @param msg_type ptp message type.
* @param port_num port number.
* @param frame frame to send.
* @param length frame length.
* @return ptp error code.
*/
int ptp_send(struct packet_ctx *ctx, int msg_type, int port_num,
             char *frame, int length);

/**
* Function for receiving PTP frames. Function can be used to poll PTP ports
* and if no frames are available, error code PTP_ERROR_TIMEOUT is returned.
* @param ctx packet if context
* @param timeout receive timeout in microseconds, decremented with used time. 
* @param port_num port number.
* @param frame buffer for received frame.
* @param length frame buffer length.
* @param recv_time timestamp for received frame.
* @param peer_addr Peer address returned here if not NULL.
* @return ptp error code.
*/
int ptp_receive(struct packet_ctx *ctx, u32 * timeout, int *port_num,
                char *frame, int *length, struct Timestamp *recv_time,
                char *peer_addr);

/** These API functions are called by packet module and implemented by 
* PTP module. 
*/

/**
* Function for reporting new PTP port. After completion of this function call,
* PTP module may start sending and receiving to this port.
* @see port_num 
* @param port_num port number.
* @param identity clock identity.
* @param set if unicast port.
* @param if_config Interface configuration.
*/
void ptp_new_port(int port_num, 
                  ClockIdentity identity, 
                  bool unicast_port,
                  struct interface_config* if_config );

/**
* Function for closing existing PTP port. After completion of this function call,
* PTP module must stop sending and receiving to this port. All pending send
* operations are completed with frame_sent.
* @see port_num 
* @param port_num port number.
*/
void ptp_close_port(int port_num);

/**
* Function for reporting the completion of sending of the PTP event 
* frame. Called for Delay_req and Sync frames (if TWO_STEP_CLOCK). 
* @see send.
* @param port_num port number.
* @param msg_hdr Header of the sent frame.
* @param error sending process failed, ptp error number returned.
* @param sent_time timestamp of the sent ptp frame.
*/
void ptp_frame_sent(int port_num, struct ptp_header *msg_hdr, int error,
                    struct Timestamp *sent_time);

#endif                          //_PACKET_IF_H_
