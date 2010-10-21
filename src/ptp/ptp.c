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
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

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
int socket_restart = 0;

#define FRAME_LEN 500

// Local data
static int trigger_reconfiguration = 0;
static struct sigaction sigaction_data;
static int daemon_running = 1;

// Local functions
static void signal_handler(int signal_number);
static void reconfig_ptp();

/**
 * Help.
 */
static void print_help(char *function)
{
    printf("%s <parameters>\n", function);
    printf("  -c <file>\t\tClock interface configuration file\n");
    printf("  -p <file>\t\tPacket interface configuration file\n");
    printf("  -o <file>\t\tOS interface configuration file\n");
    printf("  -D\t\t\tEnable logging debug messages\n");
    printf("  -h\t\t\tThis help\n");
    printf("Signals\n");
    printf("  USR1\t\t\tenable logging debug messages\n");
    printf("  HUP\t\t\ttrigger reconfiguration\n");
}

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
    char peer_ip[IP_STR_MAX_LEN];
    struct Timestamp time;
    int len = FRAME_LEN;
    int port_num = 0;
    struct Timestamp current_time, prev_time, next_time, tmp_time;
    u32 min_timeout_usec = 0;
    int debug = 0;
    int daemonize = 0;
    char c;
    pid_t pid, sid;

    // Clean context
    memset(&ptp_ctx, 0, sizeof(struct ptp_ctx));

    openlog("openptp", LOG_PID, LOG_DAEMON);

    // parse command line arguments
    while ((c = getopt(argc, argv, "c:p:o:fDh")) != -1) {
        switch (c) {
        case 'c': // Clock if configuration file
            ptp_ctx.clock_if_file = strdup(optarg);
            break;
        case 'p': // Packet if configuration file
            ptp_ctx.packet_if_file = strdup(optarg);
            break;
        case 'o': // OS if configuration file
            ptp_ctx.os_if_file = strdup(optarg);
            break;
        case 'f': // fork as daemon
            daemonize = 1;
            break;
        case 'D': // debug
            debug = 1;
            break;
        case 'h':              // help
        default:
            print_help(argv[0]);
            return;
        }
    }

    // daemonize
    if (daemonize == 1){
        pid = fork();
        if (pid < 0) {
            ERROR("fork\n");
            exit(EXIT_FAILURE);
        }
        // Exit parent
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
        
        // Change file mask
        umask(0);
        
        // Create a new SID for the child 
        sid = setsid();
        if (sid < 0) {
            ERROR("setsid\n");
            exit(EXIT_FAILURE);
        }
        
        // Change working dir
        if ((chdir("/")) < 0) {
            ERROR("chdir\n");
            exit(EXIT_FAILURE);
        }
        
        // Redirect standard streams to /dev/null
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        dup(STDOUT_FILENO);
    }

    if (optind < argc ) {
        ptp_ctx.ptp_cfg_file = strdup(argv[optind]);
    } 

    memset(&ptp_cfg, 0, sizeof(struct ptp_config));
    ptp_cfg.debug = debug;

    // Add signal handler
    memset(&sigaction_data, 0, sizeof(struct sigaction));
    sigaction_data.sa_handler = signal_handler;
    
    if (sigaction(SIGUSR1, &sigaction_data, NULL) != 0) {
        ERROR("Register signal handler failed\n");
    }
    if (sigaction(SIGHUP, &sigaction_data, NULL) != 0) {
        ERROR("Register signal handler failed\n");
    }
    if (sigaction(SIGINT, &sigaction_data, NULL) != 0) {
        ERROR("Register signal handler failed\n");
    }
    if (sigaction(SIGTERM, &sigaction_data, NULL) != 0) {
        ERROR("Register signal handler failed\n");
    }

    if( ptp_ctx.ptp_cfg_file ){
        ret = read_initialization(ptp_ctx.ptp_cfg_file);
        if (ret != PTP_ERR_OK) {
            ERROR("CFG file read %s\n", ptp_ctx.ptp_cfg_file);
        }
    }

    // Init all context
    ptp_ctx.ports_list_head = 0;
    memset(&ptp_ctx.default_dataset, 0, sizeof(struct DefaultDataSet));
    init_default_dataset(&ptp_ctx.default_dataset);

    ret = ptp_initialize_packet_if(&ptp_ctx.pkt_ctx, 
                                   ptp_ctx.packet_if_file);
    if (ret != 0) {
        ERROR("ptp_initialize_packet_if\n");
        return;
    }
    ret = ptp_initialize_os_if(&ptp_ctx.os_ctx,
                               ptp_ctx.os_if_file);
    if (ret != 0) {
        ERROR("initialize_os_if\n");
        return;
    }

    ret = ptp_initialize_clock_if(&ptp_ctx.clk_ctx, 
                                  ptp_ctx.clock_if_file);
    if (ret != 0) {
        ERROR("ptp_initialize_clock_if\n");
        return;
    }

    init_current_dataset(&ptp_ctx.current_dataset);
    init_parent_dataset(&ptp_ctx.parent_dataset);
    init_time_dataset(&ptp_ctx.time_dataset);
    init_sec_dataset(&ptp_ctx.sec_dataset);

    // Init prev time
    ptp_get_time(&ptp_ctx.clk_ctx, &prev_time);

    while (daemon_running) {
        ptp_get_time(&ptp_ctx.clk_ctx, &current_time);
        
        // Check if current time has updated to older,
        // if yes, clock has been set backwards and we must
        // restart port state to get timers fixed to new time         
        if( older_timestamp(&prev_time, &current_time) ==
            &current_time ){
            DEBUG("Clock has gone to history, restart ports\n");
            for (port = ptp_ctx.ports_list_head; port != NULL;
                 port = port->next) {
                 ptp_port_state_update(port, PORT_INITIALIZING);
            }
        }
        copy_timestamp(&prev_time, &current_time);

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
                           &port_num, frame, &len, &time,
                           peer_ip ) == PTP_ERR_OK) {
            for (port = ptp_ctx.ports_list_head;
                 port != NULL; port = port->next) {
                if (port->port_dataset.port_identity.port_number ==
                    port_num) {
                    ptp_port_recv(port, frame, len, &time, peer_ip);
                    break;
                }
            }
            if (port == NULL) {
                ERROR("frame from unconfigured port %i\n", port_num);
            }

            ptp_get_time(&ptp_ctx.clk_ctx, &current_time);
            // Check if current time has updated to older,
            // if yes, break out the receive loop
            if( older_timestamp(&prev_time, &current_time) ==
                &current_time ){
                DEBUG("Clock has gone to history, break\n");
                break;
            }
            timeout(&current_time, &next_time, &tmp_time);
            DEBUG("Timeout [%us %uns]\n",
                  (u32) tmp_time.seconds, 
                  tmp_time.nanoseconds);
            min_timeout_usec =
                tmp_time.seconds * 1000000 + tmp_time.nanoseconds / 1000;
            len = FRAME_LEN;
        }
        // Check if reconfiguration request is pending
        if( trigger_reconfiguration ){
            reconfig_ptp();
            trigger_reconfiguration = 0;
        } 
        // Check if socket is broken
        if( socket_restart ){
            ptp_close_packet_if(&ptp_ctx.pkt_ctx);
            ptp_initialize_packet_if(&ptp_ctx.pkt_ctx, 
                                     ptp_ctx.packet_if_file);
            socket_restart = 0;
        }
    }
    
    ptp_close_clock_if(&ptp_ctx.clk_ctx);
    ptp_close_os_if(&ptp_ctx.os_ctx);
    ptp_close_packet_if(&ptp_ctx.pkt_ctx);
    
    DEBUG("PTP closed\n");
}

/**
 * Reconfiguration signal handler.
 * @param signal_number Signal number.
 */
static void signal_handler(int signal_number)
{
    switch(signal_number){
    case SIGUSR1:
        ptp_cfg.debug = 1;
        break;
    case SIGHUP:
        trigger_reconfiguration = 1;
        break;
    default:
        daemon_running = 0;
        break;
    }
}

/**
 * Do reconfiguration.
 */
static void reconfig_ptp()
{
    struct ptp_port_ctx *ctx = 0;
    int ret = 0;

    if( ptp_ctx.ptp_cfg_file ){
        DEBUG("Reconfig PTP\n");
        ret = read_initialization(ptp_ctx.ptp_cfg_file);
        if (ret != PTP_ERR_OK) {
            ERROR("CFG file read %s\n", ptp_ctx.ptp_cfg_file);
        }
    }

    // Init default dataset
    init_default_dataset(&ptp_ctx.default_dataset);

    ret = ptp_reconfig_packet_if(&ptp_ctx.pkt_ctx, 
                                 ptp_ctx.packet_if_file);
    if (ret != 0) {
        ERROR("ptp_reconfig_packet_if\n");
    }

    ret = ptp_reconfig_os_if(&ptp_ctx.os_ctx,
                             ptp_ctx.os_if_file);
    if (ret != 0) {
        ERROR("ptp_reconfig_os_if\n");
    }

    ret = ptp_reconfig_clock_if(&ptp_ctx.clk_ctx, 
                                ptp_ctx.clock_if_file);
    if (ret != 0) {
        ERROR("ptp_reconfig_clock_if\n");
    }

    init_current_dataset(&ptp_ctx.current_dataset);
    init_parent_dataset(&ptp_ctx.parent_dataset);
    init_time_dataset(&ptp_ctx.time_dataset);
    init_sec_dataset(&ptp_ctx.sec_dataset);

    ctx = ptp_ctx.ports_list_head;
    while (ctx != NULL) {
        init_port_dataset( &ctx->port_dataset );
        ptp_port_state_update(ctx, PORT_INITIALIZING);
        ctx = ctx->next;
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
void add_correction(struct Timestamp *base, s64 add)
{
    u64 subn = 0, tmp = 0;

    DEBUG("Add 0x%llx/%lli\n", add, add);
    DEBUG("To  0x%llxs 0x%x.%04x\n",
          base->seconds, 
          base->nanoseconds, 
          base->frac_nanoseconds);

    if( add == 0 ){
        // No correction to add 
        return;
    }

    // Convert nanosecond and frac_nsec part of Timestamp to u64 
    subn = (((u64)base->nanoseconds) << 16) + (u64)base->frac_nanoseconds;

    // if addition is negative
    if( add < 0 ){ 
        // to prevent overflow (63 bit correction), use u64 for calculations
        tmp = (u64)(-add);
        while( tmp > subn ){
            // Add one second to subn, and decrement timestamp with one second
            // NOTE: it is not possible to subn to overflow here
            subn += ( ((u64)SEC_IN_NS) << 16 );
            base->seconds -= 1;
        }
        // Now subn is bigger than tmp, we can safely decrement tmp from subn
        subn -= tmp;
    }
    else { // positive addition
        subn += add;
    }

    // Get back to Timestamp format, first convert nanoseconds part
    while( (subn >> 16) > SEC_IN_NS ){
        base->seconds += 1;
        subn -= ( ((u64)SEC_IN_NS) << 16 );
    }
    base->nanoseconds = subn >> 16;
    // The rest is fractional subnanoseconds.
    base->frac_nanoseconds = subn & 0xffff;

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
    if (t1->frac_nanoseconds < t2->frac_nanoseconds) {
        return t1;
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
* Init port dataset.
* @param port_dataset dataset.
*/
void init_port_dataset(struct PortDataSet *port_dataset)
{
    port_dataset->log_mean_announce_interval = ptp_cfg.announce_interval;
    port_dataset->log_mean_sync_interval = ptp_cfg.sync_interval;
    port_dataset->log_min_mean_delay_req_interval = ptp_cfg.delay_req_interval;
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
