/** @file ptp_os.c
* Linux OS specific functions.
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
#include <os_if.h>
#include <ptp_general.h>

/**
 * Private data.
 */
struct private_os_if {
};

static struct private_os_if oif_data;

/**
* Function for initializing OS interface. 
* @param ctx clock context
* @return ptp error code.
*/
int initialize_os_if(struct os_ctx *ctx)
{
    struct private_os_if *oif = &oif_data;
    memset(ctx, 0, sizeof(struct os_ctx));
    memset(oif, 0, sizeof(struct private_os_if));
    ctx->arg = oif;

    return PTP_ERR_OK;
}

/**
* Function for closing OS interface. 
* @param ctx clock context
* @return ptp error code.
*/
int close_os_if(struct os_ctx *ctx)
{
//    struct private_os_if *oif = (struct private_os_if*) ctx->arg;

    return PTP_ERR_OK;
}

/** 
* Because the timestamp is implemented with 64 bit seconds field,
* the following macro is used to get 10-byte representation of the 
* timestamp in network byte order.
* @param time PTP timestamp
* @param buf buffer where to write timestamp.
*/
void ptp_format_timestamp(struct Timestamp *time, u8 * buf)
{
    u32 tmp2 = 0;
    u16 tmp1 = 0;

    tmp2 = *((u32 *) & (time->seconds));
    tmp1 = *((u16 *) (((u32 *) & (time->seconds)) + 1));
    tmp1 = htonl(tmp1);
    tmp2 = htonl(tmp2);
    memcpy(&buf[0], &tmp1, 2);
    memcpy(&buf[2], &tmp2, 4);

    tmp2 = *((u32 *) & (time->nanoseconds));
    tmp2 = htonl(tmp2);
    memcpy(&buf[6], &tmp2, 4);
}

/** 
* Because the timestamp is implemented with 64 bit seconds field,
* the following macro is used to convert 10-byte representation of the 
* timestamp in network byte order to struct Timestamp.
* @param time PTP timestamp
* @param buf timestamp in 10-byte network-byte-order.
*/
void ptp_convert_timestamp(struct Timestamp *time, u8 * buf)
{
    u32 tmp2 = 0;
    u16 tmp1 = 0;

    time->seconds = 0;
    memcpy(&tmp1, &buf[0], 2);
    memcpy(&tmp2, &buf[2], 4);
    tmp1 = ntohl(tmp1);
    tmp2 = ntohl(tmp2);
    *((u32 *) & (time->seconds)) = tmp2;
    *((u16 *) (((u32 *) & (time->seconds)) + 1)) = tmp1;

    time->nanoseconds = 0;
    memcpy(&tmp2, &buf[6], 4);
    tmp2 = ntohl(tmp2);
    *((u32 *) & (time->nanoseconds)) = tmp2;

    time->frac_nanoseconds = 0; // unused
}

/** 
* Get random number. NOTE: this is best effort, no good randominess quaranteed.
* @param min minimum value for random number.
* @param max maximum value for random number.
* @return random number between requested values.
*/
int ptp_random(int min, int max)
{
    int tmp = min;
    int fp = open("/dev/urandom", O_RDONLY);
    if (fp == 0) {
        perror("open");
    } else {
        read(fp, &tmp, sizeof(int));
        // Get it positive
        if (tmp < 0) {
            tmp = -tmp;
        }
        // rescale
        tmp %= (max - min);
        tmp += min;

        close(fp);
    }

    DEBUG("random %i < [%i] < %i\n", min, tmp, max);

    return tmp;
}

/** 
* Return bigger value.
* @param v1 value 1.
* @param v2 value 2.
* @return bigger value.
*/
int ptp_max(int v1, int v2)
{
    if (v1 > v2) {
        return v1;
    }
    return v2;
}
