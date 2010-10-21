/** @file ptp_bmc.c
* PTP best master selection algorithm.
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
#include <ptp_general.h>
#include <ptp_message.h>

#include "ptp.h"
#include "ptp_port.h"
#include "ptp_internal.h"
#include "ptp_bmc.h"
#include "ptp_framer.h"

// Data comparison functions
static struct ForeignMasterDataSet *ptp_bmc_select_best(struct
                                                        ForeignMasterDataSetElem_p
                                                        *list_head);
static int AnnounceDataComparison(struct ptp_announce *msgA,
                                  struct PortIdentity *portA,
                                  struct ptp_announce *msgB,
                                  struct PortIdentity *portB);
// State update function
static void ptp_bmc_update(struct ptp_ctx *ptp_ctx,
                           struct ptp_port_ctx *ctx,
                           enum BMCUpdate bmc_update,
                           struct ForeignMasterDataSet *foreign_bes);

/**
* PTP best master selection algorithm.
* @param ptp_ctx main context.
*/
void ptp_bmc_run(struct ptp_ctx *ptp_ctx)
{
    struct ptp_port_ctx *port = NULL;
    struct ForeignMasterDataSet *foreign_best = 0;
    struct ForeignMasterDataSetElem_p foreign_elem_p[MAX_NUM_INTERFACES];
    int num_foreign = 0;
    char tmpbuf[MAX_PTP_FRAME_SIZE];
    struct ptp_announce *D0 = 0;
    int ret = 0;
    int index = 0;

    if (ptp_ctx == 0) {
        return;
    }
    if (ptp_ctx->ports_list_head == 0) {
        return;
    }
    for (port = ptp_ctx->ports_list_head; port != NULL; port = port->next) {
        if (port->port_dataset.port_state == PORT_INITIALIZING) {
            // No BMC is done when any port is in the INITIALIZING state.   
            return;
        }
    }

    memset(&foreign_elem_p, 0, sizeof(foreign_elem_p));

    // Create D0 annouce message for BMC purposes
    ret = create_announce(ptp_ctx->ports_list_head, tmpbuf, 0, 1);
    if (ret <= 0) {
        ERROR("D0 announce creation failed\n");
        return;
    }
    D0 = (struct ptp_announce *) tmpbuf;

    for (port = ptp_ctx->ports_list_head; port != NULL; port = port->next) {
        DEBUG("%p %p\n", port, port->foreign_master_head);
        if (num_foreign >= MAX_NUM_INTERFACES) {
            // This is sanity, should never happen 
            break;
        }
        // Select Erbest (best foreign for every port) if 
        // port is not DISABLED or FAULTY
        if ((port->port_dataset.port_state == PORT_DISABLED) ||
            (port->port_dataset.port_state == PORT_FAULTY)) {
            continue;
        }
        foreign_elem_p[num_foreign].data_p =
            ptp_bmc_select_best((struct ForeignMasterDataSetElem_p *)
                                port->foreign_master_head);
        num_foreign++;
    }
    // Select Ebest (Best master) from the group of Erbest (best of every port)
    foreign_best = ptp_bmc_select_best(foreign_elem_p);

    if (foreign_best) {
        DEBUG("Ebest: %s\n",
              ptp_clk_id(foreign_best->msg.hdr.src_port_id.
                         clock_identity));
    }
    // State decision algorithm for every port
    port = ptp_ctx->ports_list_head;
    for (index = 0; index < num_foreign; index++, port = port->next) {
        // (Erbest is the empty set) AND (Port state is LISTENING)
        if ((foreign_elem_p[index].data_p == NULL) && (port->port_dataset.port_state == PORT_LISTENING) && (!port->announce_recv_timer_expired)) {      // to MASTER state?
            // YES, remain in LISTENING state
            continue;
        }

        /* Check if D0 is Class 1 through 127, and not 
         * synchronized to a foreign clock 
         * using local clock as a clock source */
        if ((ptp_ctx->parent_dataset.grandmaster_clock_quality.clock_class
             >= 1) &&
            (ptp_ctx->parent_dataset.grandmaster_clock_quality.clock_class
             <= 127) &&
            (ptp_ctx->clock_state == PTP_STATE_LOCAL_MASTER_CLOCK)) {
            // YES

            // D0 better or better by topology than Erbest ?
            if (foreign_elem_p[index].data_p) {
                DEBUG("AnnounceDataComparison1\n");
                ret = AnnounceDataComparison(D0, NULL,
                                             &foreign_elem_p[index].
                                             data_p->msg,
                                             &foreign_elem_p[index].
                                             data_p->dst_port_id);
            } else {
                // No Erbest available -> D0 is better!
                ret = 0;
            }
            if (ret < 0) {
                ERROR("AnnounceDataComparison ERROR %i\n", ret);
                continue;
            }
            if ((ret == 0) || (ret == 1)) {     // Yes -> Master
                ptp_bmc_update(ptp_ctx, port, BMC_MASTER_M1, NULL);     // M1 
                DEBUG("Port %i M1 BMC_MASTER\n", index);
            } else {            // No -> Passive
                ptp_bmc_update(ptp_ctx, port, BMC_PASSIVE_P1, foreign_elem_p[index].data_p);    // P1
                DEBUG("Port %i P1 BMC_PASSIVE\n", index);
            }
        }
        // NO
        else {
            // D0 better or better by topology than Ebest ?
            if (foreign_best) {
                DEBUG("AnnounceDataComparison2\n");
                ret = AnnounceDataComparison(D0, NULL,
                                             &foreign_best->msg,
                                             &foreign_best->dst_port_id);
            } else {
                ret = 0;        // no foreign available -> DO is best   
            }
            if (ret < 0) {
                ERROR("AnnounceDataComparison ERROR %i\n", ret);
                continue;
            }
            if ((ret == 0) || (ret == 1)) {     // Yes -> Master
                ptp_bmc_update(ptp_ctx, port, BMC_MASTER_M2, NULL);     // M2 
                DEBUG("Port %i M2 BMC_MASTER\n", index);
            } else {            // NO 
                // Ebest received on port R ?
                if (foreign_best == foreign_elem_p[index].data_p) {
                    // YES
                    ptp_bmc_update(ptp_ctx, port, BMC_SLAVE_S1, foreign_best);  // S1 
                    DEBUG("Port %i S1 BMC_SLAVE\n", index);
                } else {
                    // NO
                    if (foreign_elem_p[index].data_p) {
                        DEBUG("AnnounceDataComparison3\n");
                        // Ebest better by topology than Erbest
                        ret = AnnounceDataComparison(&foreign_best->msg,
                                                     &foreign_best->
                                                     dst_port_id,
                                                     &foreign_elem_p
                                                     [index].data_p->msg,
                                                     &foreign_elem_p
                                                     [index].data_p->
                                                     dst_port_id);
                    } else {
                        // No Erbest available -> foreign_best is better!
                        ret = 0;
                    }
                    if (ret < 0) {
                        ERROR("AnnounceDataComparison ERROR %i\n", ret);
                        continue;
                    }
                    if (ret == 1) {
                        // YES
                        ptp_bmc_update(ptp_ctx, port, BMC_PASSIVE_P2, foreign_best);    // P2
                        DEBUG("Port %i P2 BMC_PASSIVE\n", index);
                    } else {
                        // NO
                        ptp_bmc_update(ptp_ctx, port, BMC_MASTER_M3, NULL);     // M3 
                        DEBUG("Port %i M3 BMC_MASTER\n", index);
                    }
                }
            }
        }
    }
}

/**
* BMC inputs for updating the status of the whole stack.
* @param ptp_ctx PTP context.
* @param port_ctx Port context.
* @param new_state new bmc input.
* @param master if BMC_SLAVE, contains master ClockIdentity, otherwise NULL.
*/
static void ptp_bmc_update(struct ptp_ctx *ptp_ctx,
                           struct ptp_port_ctx *port_ctx,
                           enum BMCUpdate bmc_update,
                           struct ForeignMasterDataSet *foreign_best)
{
    int state_updated = 0;
    int ret = 0;

    // Inform port
    if (foreign_best) {
        state_updated =
            ptp_port_bmc_update(port_ctx, bmc_update,
                                foreign_best->src_port_id.clock_identity,
                                foreign_best->src_ip);
    } else {
        state_updated = ptp_port_bmc_update(port_ctx, bmc_update, NULL, NULL);
    }

    // Update changing dataset
    switch (bmc_update) {
    case BMC_MASTER_M1:
    case BMC_MASTER_M2:
        // Update possible changes
        ptp_ctx->parent_dataset.grandmaster_clock_quality.clock_class =
            ptp_ctx->default_dataset.clock_quality.clock_class;
        ptp_ctx->parent_dataset.grandmaster_clock_quality.clock_accuracy =
            ptp_ctx->default_dataset.clock_quality.clock_accuracy;
        ptp_ctx->parent_dataset.grandmaster_clock_quality.
            offset_scaled_log_variance =
            ptp_ctx->default_dataset.clock_quality.
            offset_scaled_log_variance;
        ptp_ctx->parent_dataset.grandmaster_priority1 =
            ptp_ctx->default_dataset.priority1;
        ptp_ctx->parent_dataset.grandmaster_priority2 =
            ptp_ctx->default_dataset.priority2;

        // Time properities dataset is updated according 
        // to local current time source
        ret = ptp_get_clock_properities(&ptp_ctx->clk_ctx,
                                        &ptp_ctx->time_dataset);
        if (ret != PTP_ERR_OK) {
            // At least, init to defauts..
            init_time_dataset(&ptp_ctx->time_dataset);
        }
        break;
    case BMC_SLAVE_S1:
        // Current dataset
        ptp_ctx->current_dataset.steps_removed =
            1 + ntohs(foreign_best->msg.steps_removed);

        // Parent dataset
        memcpy(ptp_ctx->parent_dataset.parent_port_identity.clock_identity,
               foreign_best->src_port_id.clock_identity,
               sizeof(ClockIdentity));
        ptp_ctx->parent_dataset.parent_port_identity.port_number =
            foreign_best->src_port_id.port_number;
        memcpy(ptp_ctx->parent_dataset.grandmaster_identity,
               foreign_best->msg.grandmasterId, sizeof(ClockIdentity));
        ptp_ctx->parent_dataset.grandmaster_clock_quality.clock_class =
            foreign_best->msg.grandmasterClkQuality.clock_class;
        ptp_ctx->parent_dataset.grandmaster_clock_quality.clock_accuracy =
            foreign_best->msg.grandmasterClkQuality.clock_accuracy;
        ptp_ctx->parent_dataset.grandmaster_clock_quality.
            offset_scaled_log_variance =
            ntohs(foreign_best->msg.grandmasterClkQuality.
                  offset_scaled_log_variance);
        ptp_ctx->parent_dataset.grandmaster_priority1 =
            foreign_best->msg.grandmasterPri1;
        ptp_ctx->parent_dataset.grandmaster_priority2 =
            foreign_best->msg.grandmasterPri2;

        // Time Properities dataset
        ptp_ctx->time_dataset.current_utc_offset =
            ntohs(foreign_best->msg.current_UTC_offset);
        ptp_ctx->time_dataset.current_utc_offset_valid =
            (foreign_best->msg.hdr.
             flags & PTP_UTC_OFFSET_VALID) ? true : false;
        ptp_ctx->time_dataset.leap_59 =
            (foreign_best->msg.hdr.flags & PTP_LI_59) ? true : false;
        ptp_ctx->time_dataset.leap_61 =
            (foreign_best->msg.hdr.flags & PTP_LI_61) ? true : false;
        ptp_ctx->time_dataset.time_traceable =
            (foreign_best->msg.hdr.
             flags & PTP_TIME_TRACEABLE) ? true : false;
        ptp_ctx->time_dataset.frequency_traceable =
            (foreign_best->msg.hdr.
             flags & PTP_FREQ_TRACEABLE) ? true : false;
        ptp_ctx->time_dataset.ptp_timescale =
            (foreign_best->msg.hdr.flags & PTP_TIMESCALE) ? true : false;
        ptp_ctx->time_dataset.time_source = foreign_best->msg.time_source;
        break;
    default:
        break;
    }


    // If no updates -> return
    if (!state_updated) {
        return;
    }

    DEBUG("Update on port %p %i %s\n",
          port_ctx, port_ctx->port_dataset.port_identity.port_number,
          get_bmc_update_str(bmc_update));

    // Now, update datasets
    switch (bmc_update) {
    case BMC_MASTER_M1:
    case BMC_MASTER_M2:
        /* Now this device becomes master and starts using its own clock 
         * for synchronizing other devices in the network (i.e. none of 
         * the devices ports are in slave state. */
        ptp_ctx->clock_state = PTP_STATE_LOCAL_MASTER_CLOCK;

        // give clk event
        ptp_event_clk(&ptp_ctx->clk_ctx, PTP_CLK_MASTER, NULL);

        // Current dataset
        ptp_ctx->current_dataset.steps_removed = 0;
        ptp_ctx->current_dataset.offset_from_master.scaled_nanoseconds = 0;
        ptp_ctx->current_dataset.mean_path_delay.scaled_nanoseconds = 0;
        // Parent dataset (static)
        memcpy(ptp_ctx->parent_dataset.parent_port_identity.clock_identity,
               ptp_ctx->default_dataset.clock_identity,
               sizeof(ClockIdentity));
        ptp_ctx->parent_dataset.parent_port_identity.port_number = 0;
        memcpy(ptp_ctx->parent_dataset.grandmaster_identity,
               ptp_ctx->default_dataset.clock_identity,
               sizeof(ClockIdentity));
        break;
    case BMC_MASTER_M3:
        /* Individual port shall go to MASTER state, 
         * but the clock is synchronized to foreign clock. */
        break;
    case BMC_PASSIVE_P1:
        /* We won't be synchronizing to foreign clock, because our 
         * own clock is so good, but the port shall not be in
         * master state, because there is even better clock in network. */
        ptp_ctx->clock_state = PTP_STATE_LOCAL_MASTER_CLOCK;
        break;
    case BMC_PASSIVE_P2:
        /* We are synchronized on foreign clock, but not in this port.
         * There is also good enough other master in network, so we
         * remain silent.. */

        break;
    case BMC_SLAVE_S1:
        /* There is better clock in network than our local clock is,
         * so we put the port in master state and start synchronizing 
         * local clock to new master over network. */
        ptp_ctx->clock_state = PTP_STATE_FOREIGN_MASTER_CLOCK;

        // give clk event
        ptp_event_clk(&ptp_ctx->clk_ctx, PTP_MASTER_CHANGED, NULL);

        break;
    default:
        ERROR("BMC\n");
        break;
    }
}


/**
* BMC comparison algorithm.
* @param list_head list head to list of foreigns.
* @return best of foreigns, or NULL if empty list
*/
static struct ForeignMasterDataSet *ptp_bmc_select_best(struct
                                                        ForeignMasterDataSetElem_p
                                                        *list_head)
{
    struct ForeignMasterDataSet *foreign_best = 0, *foreign = 0;
    struct ForeignMasterDataSetElem_p *foreign_enum = NULL;
    int ret = 0;

    if (list_head == NULL) {
        return NULL;
    }
    if (list_head->data_p == NULL) {
        return NULL;
    }

    foreign_enum = list_head;
    DEBUG("%p %p %p\n",
          foreign_enum, foreign_enum->data_p, &foreign_enum->data_p->msg);
    // First one is the first candidate for best
    foreign_best = foreign_enum->data_p;
    DEBUG("Erbest candidate(first): %s\n",
          ptp_clk_id(foreign_best->msg.hdr.src_port_id.clock_identity));

    // Dataset comparison by checking all foreign records.
    /*  a) Find which clock derives its time from the better grandmaster. 
     *      Choosing this, rather than which is the better clock, is essential 
     *      for the stability of the algorithm.
     *   b) If those properties are equivalent, use tie-breaking techniques.
     * A = foreign_best.
     * B = foreign.
     */
    for (foreign_enum = foreign_enum->next;
         foreign_enum; foreign_enum = foreign_enum->next) {
        // Next candidate
        foreign = foreign_enum->data_p;
        if (foreign == NULL) {
            break;
        }
        // dataset comparison 
        ret = AnnounceDataComparison(&foreign->msg,
                                     &foreign->dst_port_id,
                                     &foreign_best->msg,
                                     &foreign_best->dst_port_id);
        if (ret < 0) {
            ERROR("AnnouceDataComparison ERROR %i\n", ret);
            return NULL;
        } else if ((ret == 0) || (ret == 1)) {
            // first was better, change foreign_best
            foreign_best = foreign;
        }
        // else foreign_best was better -> no change
    }

    DEBUG("E(r)best: %s\n",
          ptp_clk_id(foreign_best->msg.hdr.src_port_id.clock_identity));
    return foreign_best;
}


/**
* BMC data comparison algorithm.
* @param msgA announce message from peer 1.
* @param portA recv port of the msg A
* @param msgB announce message from peer 2.
* @param portB recv port of the msg B
* @return:  0 if A better than B
*           1 if A better than B by topology
*           2 if B better than A
*           3 if B better than A by topology
*           or negative error code.
*/
static int AnnounceDataComparison(struct ptp_announce *msgA,
                                  struct PortIdentity *portA,
                                  struct ptp_announce *msgB,
                                  struct PortIdentity *portB)
{
    u16 tmpA = 0, tmpB = 0;
    int ret = 0;

    if (msgA == NULL) {
        ERROR("\n");
    }
    if (msgB == NULL) {
        ERROR("\n");
    }

    /* Compare two announces, candidates A and B */
    // GM Identity of A == GM Identity of B
    if (memcmp(msgA->grandmasterId, msgB->grandmasterId,
               sizeof(ClockIdentity)) == 0) {
        // A and B have the same GM, compare A and B directly (not their GM)
        DEBUG("Compare foreign masters\n");
        DEBUG("%s\n", ptp_clk_id(msgA->hdr.src_port_id.clock_identity));
        DEBUG("%s\n", ptp_clk_id(msgB->hdr.src_port_id.clock_identity));
        // Compare Steps Removed of A and B
        tmpA = ntohs(msgA->steps_removed);
        tmpB = ntohs(msgB->steps_removed);
        // A > B + 1
        if (tmpA > (tmpB + 1)) {
            DEBUG("Erbest candidate(steps removed 1): %s\n",
                  ptp_clk_id(msgB->hdr.src_port_id.clock_identity));
            return 2;           // Return B better than A
        }
        // A + 1 < B      
        else if ((tmpA + 1) < tmpB) {
            DEBUG("Erbest candidate(steps removed 2): %s\n",
                  ptp_clk_id(msgB->hdr.src_port_id.clock_identity));
            return 0;           // Return A better than B
        }
        // A > B
        else if (tmpA > tmpB) {
            // Compare Identities of Receiver of A and Sender of A
            if (portA) {        // 
                ret = compare_clock_id(portA->clock_identity,
                                       msgA->hdr.src_port_id.
                                       clock_identity);
                if (ret == -1) {        // Receiver < Sender
                    DEBUG("Erbest candidate(steps removed 3): %s\n",
                          ptp_clk_id(msgB->hdr.src_port_id.
                                     clock_identity));
                    return 2;   // Return B better than A              
                } else if (ret == 1) {  // Receiver > Sender
                    DEBUG("Erbest candidate(steps removed 4): %s\n",
                          ptp_clk_id(msgB->hdr.src_port_id.
                                     clock_identity));
                    return 3;   // Return B better by topology than A
                } else {        // Receiver = Sender
                    return -1;  // error -1
                }
            } else {
                /* If portA is NULL, it is D0 port and no 
                 * receiver<>sender id comparison can be made */
                DEBUG("DO port, no Identity comparison\n");
                DEBUG("Erbest candidate(steps removed 7): %s\n",
                      ptp_clk_id(msgB->hdr.src_port_id.clock_identity));
                return 3;       // Return B better by topology than A
            }
        }
        // A < B      
        else if (tmpA < tmpB) {
            if (portB) {
                // Compare Identities of Receiver of B and Sender of B
                ret = compare_clock_id(portB->clock_identity,
                                       msgB->hdr.src_port_id.
                                       clock_identity);
                if (ret == -1) {        // Receiver < Sender
                    DEBUG("Erbest candidate(steps removed 5): %s\n",
                          ptp_clk_id(msgA->hdr.src_port_id.
                                     clock_identity));
                    return 0;   // Return A better than B     
                } else if (ret == 1) {  // Receiver > Sender
                    DEBUG("Erbest candidate(steps removed 6): %s\n",
                          ptp_clk_id(msgA->hdr.src_port_id.
                                     clock_identity));
                    return 1;   // Return A better by topology than B
                } else {        // Receiver = Sender
                    return -1;  // error -1
                }
            } else {
                /* If portB is NULL, it is D0 port and no 
                 * receiver<>sender id comparison can be made */
                DEBUG("DO port, no Identity comparison\n");
                DEBUG("Erbest candidate(steps removed 7): %s\n",
                      ptp_clk_id(msgA->hdr.src_port_id.clock_identity));
                return 1;       // Return A better by topology than B
            }
        } else {                // A == B
            // Compare Identities of Senders of A and B
            ret = compare_clock_id(msgA->hdr.src_port_id.clock_identity,
                                   msgB->hdr.src_port_id.clock_identity);
            if (ret == -1) {    // A < B
                DEBUG("Erbest candidate(id1): %s\n",
                      ptp_clk_id(msgA->hdr.src_port_id.clock_identity));
                // Return A better by topology than B       
                return 1;
            } else if (ret == 1) {      // A > B
                DEBUG("Erbest candidate(id2): %s\n",
                      ptp_clk_id(msgB->hdr.src_port_id.clock_identity));
                // Return B better by topology than A
                return 3;
            } else {            // A == B
                // Compare Port Numbers of Receivers of A and B       
                if (portA && portB) {
                    // A > B
                    if (portA->port_number > portB->port_number) {
                        DEBUG("Erbest candidate(id3): %s\n",
                              ptp_clk_id(msgB->hdr.src_port_id.
                                         clock_identity));
                        // Return B better by topology than A
                        return 3;
                    }
                    // A < B
                    else if (portA->port_number < portB->port_number) {
                        DEBUG("Erbest candidate(id4): %s\n",
                              ptp_clk_id(msgA->hdr.src_port_id.
                                         clock_identity));
                        // Return A better by topology than B
                        return 1;
                    }
                    // A == B
                    return -2;  // error-2 
                } else {
                    // The portX that is NULL is DO port->select it better
                    if (portA) {
                        DEBUG("Erbest candidate(id5): %s\n",
                              ptp_clk_id(msgB->hdr.src_port_id.
                                         clock_identity));
                        // Return B better by topology than A
                        return 3;
                    } else {
                        DEBUG("Erbest candidate(id4): %s\n",
                              ptp_clk_id(msgA->hdr.src_port_id.
                                         clock_identity));
                        // Return A better by topology than B
                        return 1;
                    }
                }
            }
        }
    } else {
        // A and B have different GM, choose the one which has better GM
        DEBUG("Compare grandmasters\n");
        DEBUG("%s pri1 %i clock_class %i clock_accuracy %i offset_scaled_log_variance %i pri2 %i\n", 
              ptp_clk_id(msgA->grandmasterId),
              msgA->grandmasterPri1,
              msgA->grandmasterClkQuality.clock_class,
              msgA->grandmasterClkQuality.clock_accuracy,
              msgA->grandmasterClkQuality.offset_scaled_log_variance,
              msgA->grandmasterPri2 );
        DEBUG("%s pri1 %i clock_class %i clock_accuracy %i offset_scaled_log_variance %i pri2 %i\n", 
              ptp_clk_id(msgB->grandmasterId),
              msgB->grandmasterPri1,
              msgB->grandmasterClkQuality.clock_class,
              msgB->grandmasterClkQuality.clock_accuracy,
              msgB->grandmasterClkQuality.offset_scaled_log_variance,
              msgB->grandmasterPri2 );


        // Compare GM priority1 values of A and B
        if (msgA->grandmasterPri1 != msgB->grandmasterPri1) {
            if (msgA->grandmasterPri1 < msgB->grandmasterPri1) {
                DEBUG("Erbest candidate(pri1): %s %i\n",
                      ptp_clk_id(msgB->hdr.src_port_id.clock_identity),
                      msgB->grandmasterPri1);
                return 0;       // Return A better than B
            }
            DEBUG("Erbest candidate(pri1): %s %i\n",
                  ptp_clk_id(msgA->hdr.src_port_id.clock_identity),
                  msgB->grandmasterPri1);
            return 2;           // Return B better than A
        }
        // Compare GM class values of A and B
        else if (msgA->grandmasterClkQuality.clock_class !=
                 msgB->grandmasterClkQuality.clock_class) {
            if (msgA->grandmasterClkQuality.clock_class <
                msgB->grandmasterClkQuality.clock_class) {
                DEBUG("Erbest candidate(clk class): %s %i\n",
                      ptp_clk_id(msgB->hdr.src_port_id.clock_identity),
                      msgB->grandmasterClkQuality.clock_class);
                return 0;       // Return A better than B
            }
            DEBUG("Erbest candidate(clk class): %s %i\n",
                  ptp_clk_id(msgA->hdr.src_port_id.clock_identity),
                  msgA->grandmasterClkQuality.clock_class);
            return 2;           // Return B better than A
        }
        // Compare GM accuracy values of A and B
        if (msgA->grandmasterClkQuality.clock_accuracy !=
            msgB->grandmasterClkQuality.clock_accuracy) {
            if (msgA->grandmasterClkQuality.clock_accuracy <
                msgB->grandmasterClkQuality.clock_accuracy) {
                DEBUG("Erbest candidate(clk accuracy): %s %i\n",
                      ptp_clk_id(msgB->hdr.src_port_id.clock_identity),
                      msgB->grandmasterClkQuality.clock_accuracy);
                return 0;       // Return A better than B
            }
            DEBUG("Erbest candidate(clk accuracy): %s %i\n",
                  ptp_clk_id(msgA->hdr.src_port_id.clock_identity),
                  msgA->grandmasterClkQuality.clock_accuracy);
            return 2;           // Return B better than A
        }
        // Compare GM offsetScaledLogVariance values of A and B 
        // value in announce is u16, in network-byte-order
        if (msgA->grandmasterClkQuality.offset_scaled_log_variance !=
            msgB->grandmasterClkQuality.offset_scaled_log_variance) {
            if (ntohs
                (msgA->grandmasterClkQuality.offset_scaled_log_variance) <
                ntohs(msgB->grandmasterClkQuality.
                      offset_scaled_log_variance)) {
                DEBUG("Erbest candidate(variance): %s %i\n",
                      ptp_clk_id(msgB->hdr.src_port_id.clock_identity),
                      msgB->grandmasterClkQuality.offset_scaled_log_variance);
                return 0;       // Return A better than B
            }
            DEBUG("Erbest candidate(variance): %s %i\n",
                  ptp_clk_id(msgA->hdr.src_port_id.clock_identity),
                  msgA->grandmasterClkQuality.offset_scaled_log_variance);
            return 2;           // Return B better than A
        }
        // Compare GM priority2 values of A and B
        if (msgA->grandmasterPri2 != msgB->grandmasterPri2) {
            if (msgA->grandmasterPri2 < msgB->grandmasterPri2) {
                DEBUG("Erbest candidate(pri2): %s %i\n",
                      ptp_clk_id(msgB->hdr.src_port_id.clock_identity),
                      msgB->grandmasterPri2);
                return 0;       // Return A better than B
            }
            DEBUG("Erbest candidate(pri2): %s %i\n",
                  ptp_clk_id(msgA->hdr.src_port_id.clock_identity),
                  msgA->grandmasterPri2);
            return 2;           // Return B better than A
        }
        // Compare GM identity values of A and B
        ret = compare_clock_id(msgA->grandmasterId, msgB->grandmasterId);
        if (ret == -1) {        // A < B
            DEBUG("Erbest candidate(id): %s\n",
                  ptp_clk_id(msgA->hdr.src_port_id.clock_identity));
            return 0;           // Return A better than B
        } else if (ret == 1) {  // A > B
            DEBUG("Erbest candidate(id): %s\n",
                  ptp_clk_id(msgB->hdr.src_port_id.clock_identity));
            return 2;           // Return B better than A            
        } else {
            ERROR("BMC\n");
            return -1;
        }
    }
    // should not come here 
    return -1;
}
