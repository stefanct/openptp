/** @file ptp_clock.c
* PTP clock interface library.
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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/timex.h>

#include <clock_if.h>
#include <os_if.h>
#include <ptp_general.h>
#include <ptp_internal.h>
#include <ptp_config.h>
#include <ptp.h>
#include <print.h>
#include <time.h>               // for nanosleep

// Parameters
#define NUM_PATH_DELAY 5

/**
 * Local clock data.
 */
struct private_clk_if {
    struct Timestamp previous_master_timestamp;
    struct Timestamp previous_slave_timestamp;
    s64 trim;
    s64 prev_offset_from_master;
    s64 path_delay[NUM_PATH_DELAY];
    int path_delay_p;
    s64 offset_integral;
    long freq_tolerance;
    long tick;
};

// Local variables
static struct private_clk_if cif_data;

/**
* Function for initializing clock interface.
* @param ctx clock context
* @param cfg_file Clock interface configuration file name.
* @return ptp error code.
*/
int ptp_initialize_clock_if(struct clock_ctx *ctx, char* cfg_file)
{
    struct private_clk_if *cif = &cif_data;
    struct timex t;
    int ret = 0;

    memset(ctx, 0, sizeof(struct clock_ctx));
    memset(cif, 0, sizeof(struct private_clk_if));
    ctx->arg = cif;

    t.modes = ADJ_FREQUENCY;
    t.freq = 0;
    DEBUG("adjtimex 0x%08lx\n", t.freq);
    ret = adjtimex(&t);
    if (ret == -1) {
        perror("adjtimex");
    } else {
        cif->freq_tolerance = t.tolerance;
        cif->tick = t.tick;
        DEBUG("Clock state: %i, freq tolerance: %li\n",
              ret, cif->freq_tolerance);
    }

    return PTP_ERR_OK;
}

/**
* Function for reconfiguring clock interface.
* @param ctx clock context
* @param cfg_file Clock interface configuration file name.
* @return ptp error code.
*/
int ptp_reconfig_clock_if(struct clock_ctx *ctx, char* cfg_file)
{
//    struct private_clk_if *cif = (struct private_clk_if*) ctx->arg;

    return PTP_ERR_OK;
}

/**
* Function for closing clock interface.
* @param ctx clock context
* @return ptp error code.
*/
int ptp_close_clock_if(struct clock_ctx *ctx)
{
//    struct private_clk_if *cif = (struct private_clk_if*) ctx->arg;

    return PTP_ERR_OK;
}

/**
* An event reporting function to clock module.
* @param ctx clock context
* @param event status value indication function call reason.
* @param arg event specific argument.
*/
void ptp_event_clk(struct clock_ctx *ctx,
                   enum ptp_event_clk event, void *arg)
{
//    struct private_clk_if *cif = (struct private_clk_if*) ctx->arg;

    DEBUG("%s\n", get_ptp_event_clk_str(event));
    switch (event) {
    case PTP_MASTER_CHANGED:
        // Accept new master immediately. TODO: check master first!
        ptp_event_ctrl(PTP_MASTER_CLOCK_SELECTED, NULL);
        break;
    case PTP_CLK_MASTER:
        break;
    default:
        break;
    }
    print_clock_state(event);
}

/**
* Function for retrieving current time.
* @param ctx clock context
* @param time time in ptp format.
* @return ptp error code.
*/
int ptp_get_time(struct clock_ctx *ctx, struct Timestamp *time)
{
    int ret = 0;
    struct timeval tval;

    if (gettimeofday(&tval, 0) == 0) {
        // got time
        time->seconds = tval.tv_sec;
        time->nanoseconds = tval.tv_usec * 1000;
    } else {
        // error
        time->seconds = 0;
        time->nanoseconds = 0;
        ret = PTP_ERR_GEN;
    }

    return ret;
}

/**
* Function for retrieving local clock properities.
* @param ctx clock context
* @param time_dataset Time Properities dataset
* @return ptp error code.
*/
int ptp_get_clock_properities(struct clock_ctx *ctx,
                              struct TimeProperitiesDataSet *time_dataset)
{

    // The offset between TAI and UTC
    time_dataset->current_utc_offset = 0;
    // TRUE if current_utc_offset is valid
    time_dataset->current_utc_offset_valid = false;
    // TRUE if the last minute of the current UTC day will contain 59 seconds.
    time_dataset->leap_59 = false;
    // TRUE if the last minute of the current UTC day will contain 61 seconds.
    time_dataset->leap_61 = false;
    // TRUE if the timescale and the value of current_utc_offset
    // are traceable to a primary standard.
    time_dataset->time_traceable = false;
    // TRUE if the frequency determining the timescale
    // is traceable to a primary standard.
    time_dataset->frequency_traceable = false;
    // TRUE if the clock timescale of the grandmaster clock is PTP.
    time_dataset->ptp_timescale = true;
    // The source of time used by the grandmaster clock.
    time_dataset->time_source = ptp_cfg.clock_source;

    return PTP_ERR_OK;
}


/**
* Function for reporting received sync and follow_up timestamps.
* @param ctx clock context
* @param master_time master timestamp.
* @param slave_time slave timestamp.
*/
void ptp_sync_rcv(struct clock_ctx *ctx,
                  struct Timestamp *master_time,
                  struct Timestamp *slave_time)
{
    struct private_clk_if *cif = (struct private_clk_if *) ctx->arg;
    struct Timestamp diff;
    int sign = 0;
    s64 master_to_slave_delay = 0;
    s64 offset_from_master = 0;
    s32 offset_sec = 0, offset_usec = 0;

    DEBUG
        ("master: 0x%012llxs 0x%08x.%04xns slave: 0x%012llxs 0x%08x.%04xns\n",
         master_time->seconds, master_time->nanoseconds,
         master_time->frac_nanoseconds, slave_time->seconds,
         slave_time->nanoseconds, slave_time->frac_nanoseconds);

    sign = diff_timestamp(slave_time, master_time, &diff);

    if( diff.seconds > 1000 ){
        // Clock is completely wrong, adjust first closer to correct
        offset_sec = sign * diff.seconds;
    }
    else {
        /* if( sign == -1 ) master_time is after slave_time
         * -> master timestamp is after slave timestamp -> master clock is
         * ahead our clock,
         * then we mark offset_from_master negative, because our clock is late.
         *
         * if( sign == 1) master_time before slave_time
         * if difference is same as mean_path_delay, clock are at same time
         * then we mark offset_from_master positive, because our clock is ahead. */

        DEBUG("sync diff %i 0x%016llxs 0x%08x.%04xns\n",
              sign, diff.seconds, diff.nanoseconds, diff.frac_nanoseconds);
        master_to_slave_delay = sign * ((((((u64) diff.seconds) *
                                           1000000000ll) +
                                          (u64) diff.
                                          nanoseconds) << 16) | (((u64) diff.
                                                                  frac_nanoseconds)
                                                                 & 0xffff));
        DEBUG
            ("detected master_to_slave_delay %lli ns16, path_delay %lli ns16\n",
             master_to_slave_delay,
             ptp_ctx.current_dataset.mean_path_delay.scaled_nanoseconds);

        // calculate clock adjustment
        offset_from_master = master_to_slave_delay -
            ptp_ctx.current_dataset.mean_path_delay.scaled_nanoseconds;

        offset_sec = (s32) ((offset_from_master >> 16) / 1000000000LL);
        offset_usec = (s32) (((offset_from_master >> 16) / 1000LL) -
                             ((s64) offset_sec) * 1000000LL);
    }

    DEBUG("detected offset from master %is 0x%08llx ns16\n",
          offset_sec, offset_from_master);

    if (offset_sec || (offset_usec > 10000) || (offset_usec < -10000)) {
        /* Our clock is in completely wrong time.. Adjust it to
         * correct time with one crash. */
        struct timeval tval;
        if (gettimeofday(&tval, 0) == 0) {
            // adjust our clock
            tval.tv_sec -= offset_sec;  // offset_sec is now negative
            tval.tv_usec -= offset_usec;
            if (settimeofday(&tval, 0) != 0) {
                perror("settimeofday");
            } else {
                DEBUG("settimeofday, adjust: %is %ius\n",
                      -offset_sec, -offset_usec);
            }
        }
    } else {
        /* Time is close enough, calculate delay and do adjustment. */
        if (cif->previous_master_timestamp.seconds && cif->freq_tolerance && cif->tick) {       // If ajdtimex is not usable, skip
            s64 trim = 0, Ptrim = 0;
            int Pdiv = 30, Idiv = 1000;
            struct Timestamp control_space;
            double space_corr = 0;
            struct timex t;
            int ret = 0;

            // Calculate time difference between sync messages
            diff_timestamp(master_time, &cif->previous_master_timestamp,
                           &control_space);
            // calculate 1s/control_space correction
            space_corr = (double) 1000000000 /
                (double) (control_space.seconds * 1000000000 +
                          control_space.nanoseconds);
            // do clock trimming
            cif->offset_integral +=
                -(((offset_from_master >> 16) / Idiv) * space_corr);
            Ptrim = -(((offset_from_master >> 16) / Pdiv) * space_corr);
            trim = Ptrim + cif->offset_integral;
            DEBUG("trim(%f) %ins %ins:%ins\n",
                  space_corr,
                  (s32) (trim >> 16),
                  (s32) (Ptrim >> 16), (s32) (cif->offset_integral >> 16));

            t.modes = ADJ_FREQUENCY;
            // trim is nanoseconds in second, ie. ppb
            t.freq = trim * ((1 << 16 /*ppm */ ) / 1000 /*ppb */ );

            if ((t.freq > 0) && (t.freq > cif->freq_tolerance)) {
                // Adjust tick and restart PI
                t.modes |= ADJ_TICK;
                t.tick = cif->tick + 1;
                t.freq = 0;
                cif->offset_integral = 0;
                DEBUG("adjtimex max, set tick %li\n", t.tick);
            } else if ((t.freq < 0) && (t.freq < -cif->freq_tolerance)) {
                // Adjust tick and restart PI
                t.modes |= ADJ_TICK;
                t.tick = cif->tick - 1;
                t.freq = 0;
                cif->offset_integral = 0;
                DEBUG("adjtimex max, set tick %li\n", t.tick);
            } else {
                DEBUG("adjtimex freq: 0x%08lx\n", t.freq);
            }
            ret = adjtimex(&t);
            if (ret == -1) {
                perror("adjtimex");
            } else {
                cif->tick = t.tick;
                DEBUG("Clock state: %i, tick %li, freq %li\n",
                      ret, t.tick, t.freq);
            }
        } else {
            DEBUG("No trim %i %i\n",
                  (u32) cif->previous_master_timestamp.seconds,
                  (u32) ptp_ctx.current_dataset.mean_path_delay.
                  scaled_nanoseconds);
        }

        // store saved data
        cif->prev_offset_from_master =
            ptp_ctx.current_dataset.offset_from_master.scaled_nanoseconds;
        ptp_ctx.current_dataset.offset_from_master.scaled_nanoseconds =
            offset_from_master;
    }
    copy_timestamp(&cif->previous_master_timestamp, master_time);
    copy_timestamp(&cif->previous_slave_timestamp, slave_time);
}

/**
* Function for reporting received delay request timestamps.
* @param ctx clock context
* @param slave_time slave timestamp.
* @param master_time master timestamp.
*/
void ptp_delay_rcv(struct clock_ctx *ctx,
                   struct Timestamp *slave_time,
                   struct Timestamp *master_time)
{
    struct private_clk_if *cif = (struct private_clk_if *) ctx->arg;
    struct Timestamp diff;
    int sign = 0, i = 0, index = 0;
    s64 path_delay = 0;

    DEBUG
        ("slave: 0x%012llxs 0x%08x.%04xns master: 0x%012llxs 0x%08x.%04xns\n",
         slave_time->seconds, slave_time->nanoseconds,
         slave_time->frac_nanoseconds, master_time->seconds,
         master_time->nanoseconds, master_time->frac_nanoseconds);

    sign = diff_timestamp(master_time, slave_time, &diff);
    /* if( sign == -1 ) slave_time is after master_time.
     * -> slave timestamp is after master timestamp ->
     * master clock is ahead our clock,
     * then we mark path_delay negative, because our clock is late.
     *
     * if( sign == 1) slave_time before master_time.
     * -> we mark path_delay positive (the "normal" case). */
    DEBUG("delay diff %i 0x%016llxs 0x%08x.%04xns\n",
          sign, diff.seconds, diff.nanoseconds, diff.frac_nanoseconds);

    if (sign == -1) {
        DEBUG("Not using negative path delay\n");
        return;
    }

    if (diff.seconds > 1000) {
        DEBUG("Not using completely too large path delay\n");
        return;
    }

    // use sign to decide path_delay sign
    path_delay = sign *
        (((diff.seconds * 1000000000 + diff.nanoseconds) << 16) +
         diff.frac_nanoseconds);

    DEBUG("detected path_delay %lli\n", path_delay);

    // store to current dataset, and store previous also
    cif->path_delay[cif->path_delay_p] = path_delay;
    cif->path_delay_p++;
    cif->path_delay_p %= NUM_PATH_DELAY;

    // Calculate new value for path delay, running average
    index = cif->path_delay_p;
    path_delay = (cif->path_delay[index] +
                  cif->path_delay[(index + 1) % NUM_PATH_DELAY]) / 2;
    index++;
    index %= NUM_PATH_DELAY;
    for (i = 0; i < NUM_PATH_DELAY - 2; i++) {
        path_delay = (path_delay +
                      cif->path_delay[(index + 1) % NUM_PATH_DELAY]) / 2;
        index++;
        index %= NUM_PATH_DELAY;
    }
    ptp_ctx.current_dataset.mean_path_delay.scaled_nanoseconds =
        path_delay;
    DEBUG("stored path_delay %ins\n", (s32) (path_delay >> 16));
}
