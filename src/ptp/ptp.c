/** @file ptp.c
* PTP main control and helper functions.
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
#include <packet_if.h>
#include <ptp_internal.h>
#include <xml_parser.h>

#include "ptp.h"
#include "ptp_bmc.h"
#include "ptp_port.h"

/// Main data
struct ptp_ctx ptp_ctx;

#define FRAME_LEN 500
#define SEC_IN_NS   1000000000

/**
* PTP main.
* @param argc Number of commandline parameters.
* @param argv Commandline parameters.
*/
void ptp_main(int argc, char *argv[])
{
    struct ptp_port_ctx *port = NULL;
    int ret = 0;
    char frame[FRAME_LEN];
    struct Timestamp time;
    int len = FRAME_LEN;
    int port_num = 0;
    struct Timestamp current_time, next_time, tmp_time;
    u32 min_timeout_usec = 0;
    char *file = 0;

    if (argc != 2) {
        file = DEFAULT_CFG_FILE;
    } else {
        file = argv[1];
    }

    memset(&ptp_cfg, 0, sizeof(struct ptp_config));
    ptp_cfg.debug = 1;

    ret = read_initialization(file);
    if (ret != PTP_ERR_OK) {
        ERROR("CFG file read %s\n", file);
        return;
    }
    // Init all context
    memset(&ptp_ctx, 0, sizeof(struct ptp_ctx));
    ptp_ctx.ports_list_head = 0;

    init_default_dataset(&ptp_ctx.default_dataset);

    ret = ptp_initialize_packet_if(&ptp_ctx.pkt_ctx);
    if (ret != 0) {
        ERROR("ptp_initialize_packet_if\n");
        return;
    }
    ret = initialize_os_if(&ptp_ctx.os_ctx);
    if (ret != 0) {
        ERROR("initialize_os_if\n");
        return;
    }
    ret = ptp_initialize_clock_if(&ptp_ctx.clk_ctx, file);
    if (ret != 0) {
        ERROR("ptp_initialize_clock_if\n");
        return;
    }

    init_current_dataset(&ptp_ctx.current_dataset);
    init_parent_dataset(&ptp_ctx.parent_dataset);
    init_time_dataset(&ptp_ctx.time_dataset);
    init_sec_dataset(&ptp_ctx.sec_dataset);

    while (1) {
        ptp_get_time(&ptp_ctx.clk_ctx, &current_time);

    /** We do control loop in different phases:
        * 1. check ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES timers for every port.
        * 2. run BMC.
        * 3. run statemachine for every port.
        * 4. go waiting for new messages to arrive, or some
        *    of the timeouts expire.
        */

        for (port = ptp_ctx.ports_list_head; port != NULL;
             port = port->next) {
            // ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES check
            ptp_port_announce_recv_timeout_check(port, &current_time);
        }

        // Run best master selection
        ptp_bmc_run(&ptp_ctx);
        // Init next_time to "Announce message transmission interval", because
        // BMC must be run then at latest
        tmp_time.seconds = power2(ptp_cfg.announce_interval,
                                  &tmp_time.nanoseconds);
        copy_timestamp(&next_time, &current_time);
        inc_timestamp(&next_time, &tmp_time);

        // Run statemachine for every port
        for (port = ptp_ctx.ports_list_head; port != NULL;
             port = port->next) {
            ptp_port_statemachine(port, &current_time, &tmp_time);
            // Select which port must be served soonest
            if (older_timestamp(&tmp_time, &next_time) == &tmp_time) {
                copy_timestamp(&next_time, &tmp_time);
            }
        }
        // To get more accurate sleep times, read current time again
        ptp_get_time(&ptp_ctx.clk_ctx, &current_time);
        timeout(&current_time, &next_time, &tmp_time);
        DEBUG("Timeout [%us %uns]\n",
              (u32) tmp_time.seconds, tmp_time.nanoseconds);
        min_timeout_usec =
            tmp_time.seconds * 1000000 + tmp_time.nanoseconds / 1000;
        // ptp_receive will sleep min_timeout_usec if no data is received
        len = FRAME_LEN;
        while (ptp_receive(&ptp_ctx.pkt_ctx, &min_timeout_usec,
                           &port_num, frame, &len, &time) == PTP_ERR_OK) {
            for (port = ptp_ctx.ports_list_head;
                 port != NULL; port = port->next) {
                if (port->port_dataset.port_identity.port_number ==
                    port_num) {
                    ptp_port_recv(port, frame, len, &time);
                    break;
                }
            }
            if (port == NULL) {
                ERROR("frame from unconfigured port %i\n", port_num);
            }
            ptp_get_time(&ptp_ctx.clk_ctx, &current_time);
            timeout(&current_time, &next_time, &tmp_time);
            DEBUG("Timeout [%us %uns]\n",
                  (u32) tmp_time.seconds, tmp_time.nanoseconds);
            min_timeout_usec =
                tmp_time.seconds * 1000000 + tmp_time.nanoseconds / 1000;
            len = FRAME_LEN;
        }
    }
}

/**
* An event reporting function to main control.
* @param event status value indication function call reason.
* @param arg event specific argument.
*/
void ptp_event_ctrl(enum ptp_event_ctrl event, void *arg)
{
    struct ptp_port_ctx *port = NULL;

    DEBUG("%s\n", get_ptp_event_ctrl_str(event));
    switch (event) {
    case PTP_MASTER_CLOCK_SELECTED:
        // Only one port can be in UNCALIRATED state -> set it to SLAVE state
        for (port = ptp_ctx.ports_list_head; port != NULL;
             port = port->next) {
            if (port->port_dataset.port_state == PORT_UNCALIBRATED) {
                ptp_port_state_update(port, PORT_SLAVE);
                break;
            }
        }
        break;
    default:
        ERROR("EVENT_CTRL\n");
        break;
    }

}

/**
* Copy timestamp.
* @param t1 timestamp 1.
* @param t2 timestamp 2.
*/
void copy_timestamp(struct Timestamp *dst, struct Timestamp *src)
{
    dst->seconds = src->seconds;
    dst->nanoseconds = src->nanoseconds;
    dst->frac_nanoseconds = src->frac_nanoseconds;
}

/**
* Increment timestamp.
* @param base time.
* @param inc time to increment to base.
*/
void inc_timestamp(struct Timestamp *base, struct Timestamp *inc)
{
    u32 frac_nanoseconds = base->frac_nanoseconds + inc->frac_nanoseconds;
    if (frac_nanoseconds > 65536 - 1) {
        base->nanoseconds += 1;
    }
    base->frac_nanoseconds = frac_nanoseconds & 0xFFFF;
    base->nanoseconds += inc->nanoseconds;
    if (base->nanoseconds >= SEC_IN_NS) {
        base->seconds += 1;
        base->nanoseconds -= SEC_IN_NS;
    }
    base->seconds += inc->seconds;
}

/**
* Add nanoseconds to timestamp.
* @param base time.
* @param add time to increment to base, in 2^16 ns format.
*/
void add_correction(struct Timestamp *base, u64 add)
{
    u32 frac_nanoseconds = 0;

    DEBUG("Add 0x%llx\n", add);
    DEBUG("To  0x%llxs 0x%x.%04x\n",
          base->seconds, base->nanoseconds, base->frac_nanoseconds);

    frac_nanoseconds = base->frac_nanoseconds + (add & 0xFFFF);
    if (frac_nanoseconds > 65536 - 1) {
        base->nanoseconds += 1;
    }
    base->frac_nanoseconds = frac_nanoseconds & 0xFFFF;
    base->nanoseconds += (add >> 16) % SEC_IN_NS;
    if (base->nanoseconds >= SEC_IN_NS) {
        base->seconds += 1;
        base->nanoseconds -= SEC_IN_NS;
    }
    base->seconds += (add >> 16) / SEC_IN_NS;
    DEBUG("END 0x%llxs 0x%x.%04x\n",
          base->seconds, base->nanoseconds, base->frac_nanoseconds);
}

/**
* Decrement timestamp.
* @param base time.
* @param dec time to decrement from base.
*/
void dec_timestamp(struct Timestamp *base, struct Timestamp *dec)
{
    s32 frac_nanoseconds = base->frac_nanoseconds - dec->frac_nanoseconds;
    if (frac_nanoseconds < 0) {
        base->nanoseconds -= 1;
    }
    base->frac_nanoseconds = frac_nanoseconds & 0xFFFF;

    if (dec->nanoseconds > base->nanoseconds) {
        base->seconds -= 1;
        base->nanoseconds += SEC_IN_NS;
    }
    base->seconds -= dec->seconds;
    base->nanoseconds -= dec->nanoseconds;
}

/**
* Multiply timeout.
* @param timeout timeout value.
* @param multiplier .
*/
void mult_timeout(struct Timestamp *timeout, int multiplier)
{
    int i;
    for (i = 1; i < multiplier; i++) {
        timeout->seconds += timeout->seconds;
        timeout->nanoseconds += timeout->nanoseconds;
        if (timeout->nanoseconds >= SEC_IN_NS) {
            timeout->nanoseconds -= SEC_IN_NS;
            timeout->seconds += 1;
        }
    }
}

/**
* Calculate difference of the timestamps.
* @param t1 timestamp 1
* @param t2 timestamp 2
* @param diff difference between t1 and t2 (NOTE: always positive)
* @return sign of calculation: -1 = t2 after t1, 1 = t2 before t1.
*/
int diff_timestamp(struct Timestamp *t1,
                   struct Timestamp *t2, struct Timestamp *diff)
{
    if (older_timestamp(t1, t2) == t1) {
        // DEBUG("t2 is after t1 -> sign will be -1\n");
        copy_timestamp(diff, t2);
        dec_timestamp(diff, t1);
        return -1;
    } else {
        // DEBUG("t1 is after t2, i.e. t2 is before t1 -> sign will be 1\n");
        copy_timestamp(diff, t1);
        dec_timestamp(diff, t2);
        return 1;
    }
}

/**
* Return older timestamp.
* @param t1 timestamp 1.
* @param t2 timestamp 2.
*/
struct Timestamp *older_timestamp(struct Timestamp *t1,
                                  struct Timestamp *t2)
{
    // first, check seconds
    if (t1->seconds < t2->seconds) {
        return t1;
    }
    if (t1->seconds > t2->seconds) {
        return t2;
    }
    // no difference, check nanoseconds
    if (t1->nanoseconds < t2->nanoseconds) {
        return t1;
    }
    if (t1->nanoseconds > t2->nanoseconds) {
        return t2;
    }
    // no difference, check frac_nanoseconds
    if (t1->frac_nanoseconds > t2->frac_nanoseconds) {
        return t2;
    }
    // if t2 is older or timestamps are equal, return t2    
    return t2;
}

/**
* Calculate timeout from two timestamps.
* @param current current time.
* @param trig future moment when timeout happens.
* @param timeout delay before must timeout happens.
*/
void timeout(struct Timestamp *current,
             struct Timestamp *trig, struct Timestamp *timeout)
{
    // Sanity check, no negative timeouts allowed
    if (current->seconds > trig->seconds) {
        timeout->seconds = timeout->nanoseconds = 0;
    } else if ((current->seconds == trig->seconds) &&
               (current->nanoseconds > trig->nanoseconds)) {
        timeout->seconds = timeout->nanoseconds = 0;
    } else {
        timeout->seconds = trig->seconds - current->seconds;

        if (current->nanoseconds > trig->nanoseconds) {
            timeout->seconds--;
            timeout->nanoseconds =
                trig->nanoseconds + SEC_IN_NS - current->nanoseconds;
        } else {
            timeout->nanoseconds =
                trig->nanoseconds - current->nanoseconds;
        }
    }
}

/**
* Calculate PTP timeout (logaritmic setting).
* @param exp exponent.
* @param nanosecs nanoseconds part.
* @return seconds.
*/
u32 power2(s32 exp, u32 * nanosecs)
{
    unsigned long long tmp = 0, tmp2;
    int i = 0;

    if (exp == 0) {
        *nanosecs = 0;
        return 1;
    }
    tmp = 2;
    tmp <<= 32;
    if (exp < 0) {
        for (i = 0; i <= (-exp); i++) {
            tmp /= 2;
        }
    } else {
        for (i = 1; i < exp; i++) {
            tmp *= 2;
        }
    }

    DEBUG("%llx\n", tmp);
    tmp2 = tmp;
    tmp2 &= 0x00000000ffffffff;
    tmp2 *= SEC_IN_NS;
    *nanosecs = (u32) ((tmp2 >> 32) & 0x00000000ffffffff);

    return (u32) ((tmp >> 32) & 0x00000000ffffffff);
}

/**
* Init default dataset.
* @param default_dataset dataset.
*/
void init_default_dataset(struct DefaultDataSet *default_dataset)
{

    memset(default_dataset, 0, sizeof(struct DefaultDataSet));
    if (ptp_cfg.one_step_clock == 0) {
        default_dataset->two_step_clock = true; // TRUE for two_step clock
    } else {
        default_dataset->two_step_clock = false;
    }
    default_dataset->num_ports = 0;     // number of ports
    default_dataset->slave_only = false;        // true if only slave
    // quality of the clock 
    if (default_dataset->slave_only) {
        default_dataset->clock_quality.clock_class = 255;

    } else {
        default_dataset->clock_quality.clock_class = ptp_cfg.clock_class;
    }
    default_dataset->clock_quality.clock_accuracy = ptp_cfg.clock_accuracy;
    default_dataset->clock_quality.offset_scaled_log_variance =
        DEFAULT_OFFSET_SCALED_LOG_VARIANCE;
    default_dataset->priority1 = ptp_cfg.clock_priority1;
    default_dataset->priority2 = ptp_cfg.clock_priority2;
    default_dataset->domain = ptp_cfg.domain;
}

/**
* Init current dataset.
* @param current_dataset dataset.
*/
void init_current_dataset(struct CurrentDataSet *current_dataset)
{

    memset(current_dataset, 0, sizeof(struct CurrentDataSet));

    /* The number of communication paths traversed between the 
     * local clock and the grandmaster */
    current_dataset->steps_removed = 0;
    /* The time difference between a master and a slave as 
     * computed by the slave */
    current_dataset->offset_from_master.scaled_nanoseconds = 0;
    /* The mean propagation time between a master and slave as 
     * computed by the slave. */
    current_dataset->mean_path_delay.scaled_nanoseconds = 0;

    // TODO?
}


/**
* Init parent dataset.
* @param parent_dataset dataset.
*/
void init_parent_dataset(struct ParentDataSet *parent_dataset)
{

    memset(parent_dataset, 0, sizeof(struct ParentDataSet));
    memcpy(parent_dataset->parent_port_identity.clock_identity,
           ptp_ctx.default_dataset.clock_identity, sizeof(ClockIdentity));
    parent_dataset->parent_stats = false;
    parent_dataset->observed_parent_offset_scaled_log_variance = 0xffff;
    parent_dataset->observed_parent_clock_phase_change_rate = 0x7fffffff;
    memcpy(parent_dataset->grandmaster_identity,
           ptp_ctx.default_dataset.clock_identity, sizeof(ClockIdentity));
    parent_dataset->grandmaster_clock_quality.clock_class =
        ptp_ctx.default_dataset.clock_quality.clock_class;
    parent_dataset->grandmaster_clock_quality.clock_accuracy =
        ptp_ctx.default_dataset.clock_quality.clock_accuracy;
    parent_dataset->grandmaster_clock_quality.offset_scaled_log_variance =
        ptp_ctx.default_dataset.clock_quality.offset_scaled_log_variance;
    parent_dataset->grandmaster_priority1 =
        ptp_ctx.default_dataset.priority1;
    parent_dataset->grandmaster_priority2 =
        ptp_ctx.default_dataset.priority2;
}

/**
* Init time dataset.
* @param time_dataset dataset.
*/
void init_time_dataset(struct TimeProperitiesDataSet *time_dataset)
{
    memset(time_dataset, 0, sizeof(struct TimeProperitiesDataSet));
    // The offset between TAI and UTC
    time_dataset->current_utc_offset = 0;
    // TRUE if current_utc_offset is valid
    time_dataset->current_utc_offset_valid = false;
    // TRUE if the last minute of the current UTC day will contain 59 seconds.
    time_dataset->leap_59 = false;
    // TRUE if the last minute of the current UTC day will contain 61 seconds.
    time_dataset->leap_61 = false;
    /* TRUE if the timescale and the value of current_utc_offset 
     * are traceable to a primary standard. */
    time_dataset->time_traceable = false;
    /* TRUE if the frequency determining the timescale is 
     * traceable to a primary standard. */
    time_dataset->frequency_traceable = false;
    // TRUE if the clock timescale of the grandmaster clock is PTP. 
    time_dataset->ptp_timescale = true;
    // The source of time used by the grandmaster clock.      
    time_dataset->time_source = ptp_cfg.clock_source;
}

/**
* Init security dataset.
* @param sec_dataset dataset.
*/
void init_sec_dataset(struct SecurityDataSet *sec_dataset)
{
    memset(sec_dataset, 0, sizeof(struct SecurityDataSet));
}

/**
* Compare two clock identities. 
* @param id1 identity for peer 1.
* @param id2 identity for peer 2.
* @return 0 if equal, -1 id1 better (smaller), 1 if id2 is better.
*/
int compare_clock_id(ClockIdentity id1, ClockIdentity id2)
{
    int i = 0;

    /* Consider the most significant position in which the octets differ, 
     *  and treat the octets in that position as unsigned integers. 
     *  If the octet belong to X is smaller than the octet belonging
     *  to Y, then X < Y, otherwise X > Y. */
    for (i = 0; i < sizeof(ClockIdentity); i++) {
        if (id1[i] < id2[i]) {  // X<Y
            return -1;
        } else if (id1[i] > id2[i]) {   // X>Y
            return 1;
        }
        // else equal
    }

    return 0;                   // all octets where equal
}

// PTP event clock strings
static char *ptp_event_clk_str[] = {
    "PTP_MASTER_CHANGED",
};

/**
* Get string corresponding the PTP event.
* @param PTP event.
*/
char *get_ptp_event_clk_str(enum ptp_event_clk event)
{
    return ptp_event_clk_str[event];
}

// PTP ctrl event strings
static char *ptp_event_ctrl_str[] = {
    "PTP_MASTER_CLOCK_SELECTED",
};

/**
* Get string corresponding the PTP event.
* @param PTP event.
*/
char *get_ptp_event_ctrl_str(enum ptp_event_ctrl event)
{
    return ptp_event_ctrl_str[event];
}

// State strings
char *ptp_port_state_str[] = {
    "PORT_INITIALIZING",
    "PORT_FAULTY",
    "PORT_DISABLED",
    "PORT_LISTENING",
    "PORT_PRE_MASTER",
    "PORT_MASTER",
    "PORT_PASSIVE",
    "PORT_UNCALIBRATED",
    "PORT_SLAVE",
};

/**
* Get string corresponding the port state.
* @param state port state.
*/
char *get_state_str(enum PortState state)
{
    return ptp_port_state_str[state];
}

// BMC update strings
char *ptp_bmc_update_str[] = {
    "BMC_MASTER_M1",
    "BMC_MASTER_M1",
    "BMC_MASTER_M3",
    "BMC_PASSIVE_P1",
    "BMC_PASSIVE_P2",
    "BMC_SLAVE_S1",
};

/**
* Get string corresponding the BMC update.
* @param BMC update.
*/
char *get_bmc_update_str(enum BMCUpdate update)
{
    return ptp_bmc_update_str[update];
}
