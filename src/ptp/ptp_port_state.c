/** @file ptp_port_state.c
* PTP port statemachine.
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
#include <ptp_general.h>
#include <ptp_message.h>
#include <ptp_internal.h>
#include <os_if.h>

#include "ptp.h"
#include "ptp_port.h"
#include "ptp_framer.h"

/// static functions for handling PTP port states
static void ptp_port_state_initializing(struct ptp_port_ctx *ctx,
                                        struct Timestamp *current_time,
                                        bool enter_state);
static void ptp_port_state_faulty(struct ptp_port_ctx *ctx,
                                  struct Timestamp *current_time,
                                  bool enter_state);
static void ptp_port_state_disabled(struct ptp_port_ctx *ctx,
                                    struct Timestamp *current_time,
                                    bool enter_state);
static void ptp_port_state_listening(struct ptp_port_ctx *ctx,
                                     struct Timestamp *current_time,
                                     bool enter_state);
static void ptp_port_state_pre_master(struct ptp_port_ctx *ctx,
                                      struct Timestamp *current_time,
                                      bool enter_state);
static void ptp_port_state_master(struct ptp_port_ctx *ctx,
                                  struct Timestamp *current_time,
                                  bool enter_state);
static void ptp_port_state_passive(struct ptp_port_ctx *ctx,
                                   struct Timestamp *current_time,
                                   bool enter_state);
static void ptp_port_state_uncalibrated(struct ptp_port_ctx *ctx,
                                        struct Timestamp *current_time,
                                        bool enter_state);
static void ptp_port_state_slave(struct ptp_port_ctx *ctx,
                                 struct Timestamp *current_time,
                                 bool enter_state);

/**
* Statemachine for PTP port.
* @param ctx Port context.
* @param current_time current time.
* @param next_time when should be called next time.
*/
void ptp_port_statemachine(struct ptp_port_ctx *ctx,
                           struct Timestamp *current_time,
                           struct Timestamp *next_time)
{
    struct Timestamp timeout_tmp;
    struct Timestamp timestamp_tmp;
    struct Timestamp *timeout_p = &timeout_tmp;
    struct ForeignMasterDataSetElem *foreign_elem =
        ctx->foreign_master_head;
    struct ForeignMasterDataSetElem *foreign_elem_prev = 0, *ftmp = 0;
    struct ForeignMasterDataSet *foreign = 0;
    bool enter_state = false;
    int i = 0;

    memset(&timeout_tmp, 0, sizeof(struct Timestamp));
    memset(&timestamp_tmp, 0, sizeof(struct Timestamp));

    DEBUG("%s %us %uns\n", get_state_str(ctx->port_dataset.port_state),
          (u32) current_time->seconds, current_time->nanoseconds);

    // Check foreign records timestamps
    while (foreign_elem) {
        foreign = foreign_elem->data_p;
        // Get timeout value for announce_interval
        timeout_tmp.seconds =
            power2(foreign->msg.hdr.log_mean_msg_interval,
                   &timeout_tmp.nanoseconds);
        /* Get timeout which is the length of the announce window 
         * (announce_interval*ANNOUNCE_WINDOW) */
        mult_timeout(&timeout_tmp,
                     ctx->port_dataset.announce_receipt_timeout);
        DEBUG("Announce window %us %uns\n",
              (u32) timeout_tmp.seconds, (u32) timeout_tmp.nanoseconds);

        // initialialize timestamp to current time
        copy_timestamp(&timestamp_tmp, current_time);
        // dec timestamp with announce window timeout
        dec_timestamp(&timestamp_tmp, &timeout_tmp);

        DEBUG("Announce history %us %uns\n",
              (u32) timestamp_tmp.seconds,
              (u32) timestamp_tmp.nanoseconds);
        foreign->foreign_master_announce_messages = 0;
        for (i = 0; i < ANNOUNCE_WINDOW; i++) {
            if ((foreign->announce_tstamp[i].seconds == 0) &&
                (foreign->announce_tstamp[i].nanoseconds == 0)) {
                // invalid timestamp
                continue;
            }
            /* check announce timestamps comparing to 
             * calculated announce window */
            if (older_timestamp(&foreign->announce_tstamp[i],
                                &timestamp_tmp) == &timestamp_tmp) {
                // This is ok        
                foreign->foreign_master_announce_messages += 1;
            } else {
                DEBUG("Expired announce: %llis %ins\n",
                      foreign->announce_tstamp[i].seconds,
                      foreign->announce_tstamp[i].nanoseconds);
            }
        }
        DEBUG("Valid announces: %i\n",
              foreign->foreign_master_announce_messages);
        if (foreign->foreign_master_announce_messages == 0) {
            DEBUG("Remove foreign record ");
            for (i = 0; i < 9; i++) {
                DEBUG_PLAIN("%02x:",
                            0xff & ((char *) &foreign->src_port_id)[i]);
            }
            DEBUG_PLAIN("%02x\n",
                        0xff & ((char *) &foreign->src_port_id)[i]);

            ftmp = foreign_elem;
            foreign_elem = foreign_elem->next;
            free(ftmp);
            if (foreign_elem_prev == 0) {
                ctx->foreign_master_head = foreign_elem;
            } else {
                foreign_elem_prev->next = foreign_elem;
            }
        } else {
            // update pointers
            foreign_elem_prev = foreign_elem;
            foreign_elem = foreign_elem->next;
        }
    }

    do {
        enter_state = ctx->port_state_updated;
        ctx->port_state_updated = false;

        switch (ctx->port_dataset.port_state) {
        case PORT_INITIALIZING:
            ptp_port_state_initializing(ctx, current_time, enter_state);
            break;
        case PORT_FAULTY:
            ptp_port_state_faulty(ctx, current_time, enter_state);
            break;
        case PORT_DISABLED:
            ptp_port_state_disabled(ctx, current_time, enter_state);
            break;
        case PORT_LISTENING:
            ptp_port_state_listening(ctx, current_time, enter_state);
            break;
        case PORT_PRE_MASTER:
            ptp_port_state_pre_master(ctx, current_time, enter_state);
            break;
        case PORT_MASTER:
            ptp_port_state_master(ctx, current_time, enter_state);
            break;
        case PORT_PASSIVE:
            ptp_port_state_passive(ctx, current_time, enter_state);
            break;
        case PORT_UNCALIBRATED:
            ptp_port_state_uncalibrated(ctx, current_time, enter_state);
            break;
        case PORT_SLAVE:
            ptp_port_state_slave(ctx, current_time, enter_state);
            break;
        default:
            ERROR("Error state: %i\n", ctx->port_dataset.port_state);
            break;
        }
    } while (ctx->port_state_updated);

    // Default timeout is 120s 
    timeout_tmp.seconds = current_time->seconds + 120;
    timeout_tmp.nanoseconds = current_time->nanoseconds;

    // solve which timeout is next
    DEBUG("Timer flags: 0x%02x\n", ctx->timer_flags);
    if (ctx->timer_flags & ANNOUNCE_TIMER) {
        DEBUG("Announce timer: %llus %uns\n",
              ctx->announce_timer.seconds,
              ctx->announce_timer.nanoseconds);
        timeout_p = older_timestamp(timeout_p, &ctx->announce_timer);
    }
    if (ctx->timer_flags & SYNC_TIMER) {
        DEBUG("Sync timer: %llus %uns\n",
              ctx->sync_timer.seconds, ctx->sync_timer.nanoseconds);
        timeout_p = older_timestamp(timeout_p, &ctx->sync_timer);
    }
    if (ctx->timer_flags & DELAY_REQ_TIMER) {
        DEBUG("Delay_req timer: %llus %uns\n",
              ctx->delay_req_timer.seconds,
              ctx->delay_req_timer.nanoseconds);
        timeout_p = older_timestamp(timeout_p, &ctx->delay_req_timer);
    }
    if (ctx->timer_flags & PDELAY_REQ_TIMER) {
        DEBUG("PDelay_req timer: %llus %uns\n",
              ctx->pdelay_req_timer.seconds,
              ctx->pdelay_req_timer.nanoseconds);
        timeout_p = older_timestamp(timeout_p, &ctx->pdelay_req_timer);
    }
    if (ctx->timer_flags & ANNOUNCE_RECV_TIMER) {
        DEBUG("timeout_p: %llus %uns\n",
              timeout_p->seconds, timeout_p->nanoseconds);
        DEBUG("Announce recv timer: %llus %uns\n",
              ctx->announce_recv_timer.seconds,
              ctx->announce_recv_timer.nanoseconds);
        timeout_p = older_timestamp(timeout_p, &ctx->announce_recv_timer);
        DEBUG("timeout_p: %llus %uns\n",
              timeout_p->seconds, timeout_p->nanoseconds);
    }
    copy_timestamp(next_time, timeout_p);
    DEBUG("Next time [%llus %uns]: %llus %uns\n",
          current_time->seconds, current_time->nanoseconds,
          next_time->seconds, next_time->nanoseconds);
}

/**
* Statemachine function for state INITIALIZING.
* @param ctx Port context.
* @param current_time current time.
* @param enter_state true when called first time after state transition.
*/
static void ptp_port_state_initializing(struct ptp_port_ctx *ctx,
                                        struct Timestamp *current_time,
                                        bool enter_state)
{
    if (enter_state) {
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES stop
        ptp_port_announce_recv_timeout_stop(ctx, current_time);
    }
    ptp_port_state_update(ctx, PORT_LISTENING);
}

/**
* Statemachine function for state FAULTY.
* @param ctx Port context.
* @param current_time current time.
* @param enter_state true when called first time after state transition.
*/
static void ptp_port_state_faulty(struct ptp_port_ctx *ctx,
                                  struct Timestamp *current_time,
                                  bool enter_state)
{
    if (enter_state) {
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES stop
        ptp_port_announce_recv_timeout_stop(ctx, current_time);
    }
}

/**
* Statemachine function for state DISABLED.
* @param ctx Port context.
* @param current_time current time.
* @param enter_state true when called first time after state transition.
*/
static void ptp_port_state_disabled(struct ptp_port_ctx *ctx,
                                    struct Timestamp *current_time,
                                    bool enter_state)
{
    if (enter_state) {
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES stop
        ptp_port_announce_recv_timeout_stop(ctx, current_time);
    }
}

/**
* Statemachine function for state LISTENING.
* @param ctx Port context.
* @param current_time current time.
* @param enter_state true when called first time after state transition.
*/
static void ptp_port_state_listening(struct ptp_port_ctx *ctx,
                                     struct Timestamp *current_time,
                                     bool enter_state)
{
    if (enter_state) {
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES restart
        ptp_port_announce_recv_timeout_restart(ctx, current_time);
    }
}

/**
* Statemachine function for state PRE_MASTER.
* @param ctx Port context.
* @param current_time current time.
* @param enter_state true when called first time after state transition.
*/
static void ptp_port_state_pre_master(struct ptp_port_ctx *ctx,
                                      struct Timestamp *current_time,
                                      bool enter_state)
{
    if (enter_state) {
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES stop
        ptp_port_announce_recv_timeout_stop(ctx, current_time);
    }
    // Check if it is QUALIFICATION_TIMEOUT_EXPIRES
    if (older_timestamp(&ctx->announce_timer,
                        current_time) != current_time) {
        DEBUG("QUALIFICATION_TIMEOUT_EXPIRES %us %uns\n",
              (u32) ctx->announce_timer.seconds,
              ctx->announce_timer.nanoseconds);
        ptp_port_state_update(ctx, PORT_MASTER);
    }
}

/**
* Statemachine function for state MASTER.
* @param ctx Port context.
* @param current_time current time.
* @param enter_state true when called first time after state transition.
*/
static void ptp_port_state_master(struct ptp_port_ctx *ctx,
                                  struct Timestamp *current_time,
                                  bool enter_state)
{
    char tmpbuf[MAX_PTP_FRAME_SIZE];
    int ret = 0;
    struct Timestamp time_tmp;

    if (enter_state) {
        // reset seqid
        ctx->sync_seqid = 0;
        ctx->delay_req_seqid = 0;
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES stop
        ptp_port_announce_recv_timeout_stop(ctx, current_time);
    }
    // Check if it is time to send sync
    if (enter_state ||
        (older_timestamp(&ctx->sync_timer, current_time) !=
         current_time)) {
        DEBUG("Sync %us %uns\n", (u32) ctx->sync_timer.seconds,
              ctx->sync_timer.nanoseconds);
        // create sync
        ret = create_sync(ctx, tmpbuf, ctx->sync_seqid);
        if (ret > 0) {
            DEBUG("Send SYNC\n");
            ret = ptp_send(&ptp_ctx.pkt_ctx, PTP_SYNC,
                           ctx->port_dataset.port_identity.port_number,
                           tmpbuf, ret);
            if (ret == PTP_ERR_OK) {
                ctx->sync_seqid++;
                // sync sent succesfully, update timeout
                time_tmp.seconds =
                    power2(ctx->port_dataset.log_mean_sync_interval,
                           &time_tmp.nanoseconds);
                copy_timestamp(&ctx->sync_timer, current_time);
                inc_timestamp(&ctx->sync_timer, &time_tmp);
                DEBUG("Set Sync timeout 2^%i=%us %uns %us %uns\n",
                      ctx->port_dataset.log_mean_sync_interval,
                      (u32) time_tmp.seconds,
                      (u32) time_tmp.nanoseconds,
                      (u32) ctx->sync_timer.seconds,
                      (u32) ctx->sync_timer.nanoseconds);
                ctx->timer_flags |= SYNC_TIMER;
            }
        }
    }
    // Check if it is time to send announce
    if (enter_state ||
        (older_timestamp(&ctx->announce_timer,
                         current_time) != current_time)) {
        DEBUG("Announce %us %uns\n",
              (u32) ctx->announce_timer.seconds,
              ctx->announce_timer.nanoseconds);
        // Create and send announce
        ret = create_announce(ctx, tmpbuf, ctx->announce_seqid);
        if (ret > 0) {
            DEBUG("Send ANNOUNCE\n");
            ret = ptp_send(&ptp_ctx.pkt_ctx, PTP_ANNOUNCE,
                           ctx->port_dataset.port_identity.port_number,
                           tmpbuf, ret);
            if (ret == PTP_ERR_OK) {
                ctx->announce_seqid++;
                // announce sent succesfully, update timeout
                time_tmp.seconds =
                    power2(ctx->port_dataset.log_mean_announce_interval,
                           &time_tmp.nanoseconds);
                copy_timestamp(&ctx->announce_timer, current_time);
                inc_timestamp(&ctx->announce_timer, &time_tmp);
                DEBUG("Set Announce timeout 2^%i=%us %uns %us %uns\n",
                      ctx->port_dataset.log_mean_announce_interval,
                      (u32) time_tmp.seconds,
                      (u32) time_tmp.nanoseconds,
                      (u32) ctx->announce_timer.seconds,
                      (u32) ctx->announce_timer.nanoseconds);
                ctx->timer_flags |= ANNOUNCE_TIMER;
            }
        }
    }
}

/**
* Statemachine function for state PASSIVE.
* @param ctx Port context.
* @param current_time current time.
* @param enter_state true when called first time after state transition.
*/
static void ptp_port_state_passive(struct ptp_port_ctx *ctx,
                                   struct Timestamp *current_time,
                                   bool enter_state)
{
    if (enter_state) {
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES restart
        ptp_port_announce_recv_timeout_restart(ctx, current_time);
    }
}

/**
* Statemachine function for state UNCALIBRATED.
* @param ctx Port context.
* @param current_time current time.
* @param enter_state true when called first time after state transition.
*/
static void ptp_port_state_uncalibrated(struct ptp_port_ctx *ctx,
                                        struct Timestamp *current_time,
                                        bool enter_state)
{
    char tmpbuf[MAX_PTP_FRAME_SIZE];
    int ret = 0;
    u32 nanosecs = 0;
    u32 secs = 0;
    u32 t1_usec = 0;
    u32 t2_usec = 0;

    if (enter_state) {
        // reset seqids
        ctx->sync_seqid = 0;
        ctx->delay_req_seqid = 0;
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES restart
        ptp_port_announce_recv_timeout_restart(ctx, current_time);
    }
    // Check if it is time to send delay_req 
    if (enter_state ||
        (older_timestamp(&ctx->delay_req_timer,
                         current_time) != current_time)) {
        // create delay_req
        ret = create_delay_req(ctx, tmpbuf, ctx->delay_req_seqid);
        if (ret > 0) {
            DEBUG("Send Delay_req %i\n", ctx->delay_req_seqid);
            ret = ptp_send(&ptp_ctx.pkt_ctx, PTP_DELAY_REQ,
                           ctx->port_dataset.port_identity.port_number,
                           tmpbuf, ret);
            if (ret == PTP_ERR_OK) {
                ctx->delay_req_seqid++;
                // delay req sent succesfully, update timeout
                secs =
                    power2(ctx->port_dataset.
                           log_min_mean_delay_req_interval, &nanosecs);
                t1_usec = secs * 1000000 + nanosecs / 1000;
                secs =
                    power2(ctx->port_dataset.
                           log_min_mean_delay_req_interval + 1, &nanosecs);
                t2_usec = secs * 1000000 + nanosecs / 1000;
                t1_usec = ptp_random(t1_usec, t2_usec);

                ctx->delay_req_timer.seconds =
                    current_time->seconds + t1_usec / 1000000;
                ctx->delay_req_timer.nanoseconds =
                    current_time->nanoseconds + (t1_usec % 1000000) * 1000;
                DEBUG("Set Delay_req timeout 2^%i-2^%i=%uus %us %uns\n",
                      ctx->port_dataset.log_min_mean_delay_req_interval,
                      ctx->port_dataset.log_min_mean_delay_req_interval +
                      1, t1_usec, (u32) ctx->delay_req_timer.seconds,
                      (u32) ctx->delay_req_timer.nanoseconds);
                ctx->timer_flags |= DELAY_REQ_TIMER;
            }
        }
    }
}

/**
* Statemachine function for state SLAVE.
* @param ctx Port context.
* @param current_time current time.
* @param enter_state true when called first time after state transition.
*/
static void ptp_port_state_slave(struct ptp_port_ctx *ctx,
                                 struct Timestamp *current_time,
                                 bool enter_state)
{
    char tmpbuf[MAX_PTP_FRAME_SIZE];
    int ret = 0;
    u32 nanosecs = 0;
    u32 secs = 0;
    u32 t1_usec = 0;
    u32 t2_usec = 0;

    if (enter_state) {
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES restart
        ptp_port_announce_recv_timeout_restart(ctx, current_time);
    }
    // Check if it is time to send delay_req  
    /* Not neccessary when enter_state==true, because 
     * always entering from UNCALIBRATED
     * state which has already issued delay_reqs and updated the sync_timer. */
    if (older_timestamp(&ctx->delay_req_timer,
                        current_time) != current_time) {
        // create delay_req
        ret = create_delay_req(ctx, tmpbuf, ctx->delay_req_seqid);
        if (ret > 0) {
            DEBUG("Send Delay_req %i\n", ctx->delay_req_seqid);
            ret = ptp_send(&ptp_ctx.pkt_ctx, PTP_DELAY_REQ,
                           ctx->port_dataset.port_identity.port_number,
                           tmpbuf, ret);
            if (ret == PTP_ERR_OK) {
                ctx->delay_req_seqid++;
                // delay req sent succesfully, update timeout
                secs =
                    power2(ctx->port_dataset.
                           log_min_mean_delay_req_interval, &nanosecs);
                t1_usec = secs * 1000000 + nanosecs / 1000;
                secs =
                    power2(ctx->port_dataset.
                           log_min_mean_delay_req_interval + 1, &nanosecs);
                t2_usec = secs * 1000000 + nanosecs / 1000;
                t1_usec = ptp_random(t1_usec, t2_usec);

                ctx->delay_req_timer.seconds =
                    current_time->seconds + t1_usec / 1000000;
                ctx->delay_req_timer.nanoseconds =
                    current_time->nanoseconds + (t1_usec % 1000000) * 1000;
                DEBUG("Set Delay_req timeout 2^%i-2^%i=%uus %us %uns\n",
                      ctx->port_dataset.log_min_mean_delay_req_interval,
                      ctx->port_dataset.log_min_mean_delay_req_interval +
                      1, t1_usec, (u32) ctx->delay_req_timer.seconds,
                      (u32) ctx->delay_req_timer.nanoseconds);
                ctx->timer_flags |= DELAY_REQ_TIMER;
            }
        }
    }
}

/**
* Function for updating the PTP port state
* @param ctx Port context.
* @param new_state new statemachine state.
*/
void ptp_port_state_update(struct ptp_port_ctx *ctx,
                           enum PortState new_state)
{
    // This function should check the validity of the port state updates, 
    // and do required ctx updates.
    if (ctx->port_dataset.port_state != new_state) {
        DEBUG("from %s to %s\n",
              get_state_str(ctx->port_dataset.port_state),
              get_state_str(new_state));
        ctx->port_dataset.port_state = new_state;
        ctx->timer_flags = 0;   // Disable timers
        ctx->port_state_updated = true; // indicate that port state has updated
    }
}

/**
* BMC inputs for port state updating.
* @param ctx Port context.
* @param new_state new bmc input.
* @param master if BMC_SLAVE or BMC_PASSIVE, contains master 
*               ClockIdentity, otherwise NULL.
* @return true if state updated.
*/
bool ptp_port_bmc_update(struct ptp_port_ctx *ctx,
                         enum BMCUpdate bmc_update, ClockIdentity master)
{
    struct Timestamp current_time = { 0, 0 };
    bool state_update = false;

    if (ptp_get_time(&ptp_ctx.clk_ctx, &current_time) != PTP_ERR_OK) {
        ERROR("ptp_get_time\n");
        // No valid time
        current_time.seconds = current_time.nanoseconds = 0;
    }

    DEBUG("%s %us %uns\n",
          get_bmc_update_str(bmc_update),
          (u32) current_time.seconds, (u32) current_time.nanoseconds);

    switch (bmc_update) {
    case BMC_MASTER_M1:
    case BMC_MASTER_M2:
    case BMC_MASTER_M3:
        if (ctx->port_dataset.port_state == PORT_MASTER) {
            // Nothing to do
            break;
        }
        if ((ctx->port_dataset.port_state == PORT_LISTENING) ||
            (ctx->port_dataset.port_state == PORT_UNCALIBRATED) ||
            (ctx->port_dataset.port_state == PORT_SLAVE) ||
            (ctx->port_dataset.port_state == PORT_MASTER) ||
            (ctx->port_dataset.port_state == PORT_PASSIVE)) {

            // Entering to the master state, initiate pre_master timer   
            struct Timestamp time_tmp;
            int N = 0;
            if (bmc_update == BMC_MASTER_M3) {
                // For M3, N shall be the value incremented by 1 (one) of 
                // the steps_removed field of the current data set
                N = ptp_ctx.current_dataset.steps_removed + 1;
            }
            time_tmp.seconds =
                power2(ctx->port_dataset.log_mean_announce_interval,
                       &time_tmp.nanoseconds);
            mult_timeout(&time_tmp, N);
            copy_timestamp(&ctx->announce_timer, &current_time);
            inc_timestamp(&ctx->announce_timer, &time_tmp);
            DEBUG("Set PRE_MASTER timeout %us %uns %us %uns\n",
                  (u32) time_tmp.seconds,
                  (u32) time_tmp.nanoseconds,
                  (u32) ctx->announce_timer.seconds,
                  (u32) ctx->announce_timer.nanoseconds);

            state_update = true;
            ptp_port_state_update(ctx, PORT_PRE_MASTER);
            ctx->timer_flags |= ANNOUNCE_TIMER;
        }
        break;
    case BMC_PASSIVE_P1:
    case BMC_PASSIVE_P2:
        if (ctx->port_dataset.port_state != PORT_PASSIVE) {
            // enter passive, copy master identity for the use of 
            // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES algorithm
            memcpy(ctx->current_master, master, sizeof(ClockIdentity));
        }
        if ((ctx->port_dataset.port_state == PORT_LISTENING) ||
            (ctx->port_dataset.port_state == PORT_UNCALIBRATED) ||
            (ctx->port_dataset.port_state == PORT_SLAVE) ||
            (ctx->port_dataset.port_state == PORT_PRE_MASTER) ||
            (ctx->port_dataset.port_state == PORT_MASTER)) {
            state_update = true;
            ptp_port_state_update(ctx, PORT_PASSIVE);
        }
        break;
    case BMC_SLAVE_S1:
        if (ctx->port_dataset.port_state == PORT_SLAVE) {
            if (memcmp(ctx->current_master,
                       master, sizeof(ClockIdentity)) == 0) {
                // same master
                // nothing to do..
                break;
            } else {            // master has changed
                memcpy(ctx->current_master, master, sizeof(ClockIdentity));
                state_update = true;
                ptp_port_state_update(ctx, PORT_UNCALIBRATED);
            }
        } else if ((ctx->port_dataset.port_state == PORT_LISTENING) ||
                   (ctx->port_dataset.port_state == PORT_PRE_MASTER) ||
                   (ctx->port_dataset.port_state == PORT_MASTER) ||
                   (ctx->port_dataset.port_state == PORT_PASSIVE)) {
            memcpy(ctx->current_master, master, sizeof(ClockIdentity));
            ptp_port_state_update(ctx, PORT_UNCALIBRATED);
            state_update = true;
        } else if (ctx->port_dataset.port_state == PORT_UNCALIBRATED) {
            if (memcmp(ctx->current_master,
                       master, sizeof(ClockIdentity)) != 0) {
                // different master candidate
                memcpy(ctx->current_master, master, sizeof(ClockIdentity));
                // this is update, although port state is not changed
                state_update = true;
            }
        }
        break;
    default:
        ERROR("BMC\n");
        break;
    }
    return state_update;
}

/**
* Check if ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES.
* @param ctx Port context.
* @param current_time current time.
*/
void ptp_port_announce_recv_timeout_check(struct ptp_port_ctx *ctx,
                                          struct Timestamp *current_time)
{
    DEBUG("\n");
    if (!(ctx->timer_flags & ANNOUNCE_RECV_TIMER)) {
        DEBUG("Not active\n");
        // timer stopped, return
        return;
    }
    if (older_timestamp(&ctx->announce_recv_timer,
                        current_time) != current_time) {
        DEBUG("Expired\n");
        // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES event
        if ((ctx->port_dataset.port_state == PORT_LISTENING) ||
            (ctx->port_dataset.port_state == PORT_PASSIVE) ||
            (ctx->port_dataset.port_state == PORT_UNCALIBRATED) ||
            (ctx->port_dataset.port_state == PORT_SLAVE)) {
            // timer expired. Let the BMC algorithm do the state update.
            ctx->announce_recv_timer_expired = true;
            ctx->timer_flags &= ~ANNOUNCE_RECV_TIMER;
        } else {
            ERROR("fault state %s\n",
                  get_state_str(ctx->port_dataset.port_state));
        }
    }
}

/**
* Restart ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES timer (or start if stopped).
* @param ctx Port context.
* @param current_time current time.
*/
void ptp_port_announce_recv_timeout_restart(struct ptp_port_ctx *ctx,
                                            struct Timestamp *current_time)
{
    struct Timestamp time_tmp;

    DEBUG("\n");
    // Get timeout value
    time_tmp.seconds = power2(ctx->port_dataset.log_mean_announce_interval,
                              &time_tmp.nanoseconds);
    // multiply with number of announces per window + 0 or 1
    mult_timeout(&time_tmp,
                 ctx->port_dataset.announce_receipt_timeout +
                 ptp_random(0, 1));
    // copy current time
    copy_timestamp(&ctx->announce_recv_timer, current_time);
    // add timeout to current time
    inc_timestamp(&ctx->announce_recv_timer, &time_tmp);
    DEBUG
        ("Set ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES timeout %us %uns %us %uns\n",
         (u32) time_tmp.seconds, (u32) time_tmp.nanoseconds,
         (u32) ctx->announce_recv_timer.seconds,
         (u32) ctx->announce_recv_timer.nanoseconds);

    ctx->timer_flags |= ANNOUNCE_RECV_TIMER;
}

/**
* Stop ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES timer.
* @param ctx Port context.
* @param current_time current time.
*/
void ptp_port_announce_recv_timeout_stop(struct ptp_port_ctx *ctx,
                                         struct Timestamp *current_time)
{

    DEBUG("\n");
    // disable
    ctx->timer_flags &= ~ANNOUNCE_RECV_TIMER;
    // for safety, zero timeout values
    ctx->announce_recv_timer.seconds = 0;
    ctx->announce_recv_timer.nanoseconds = 0;
}
