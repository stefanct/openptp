/** @file os_if.h
* OS interface description.
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
#ifndef _OS_IF_H_
#define _OS_IF_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

#include "ptp_general.h"

/**
* OS context information.
*/
struct os_ctx {
    void *arg;                  ///< private data for os specific data        
};

/**
* Function for initializing OS interface. 
* @param ctx clock context
* @return ptp error code.
*/
int initialize_os_if(struct os_ctx *ctx);

/**
* Function for closing OS interface. 
* @param ctx clock context
* @return ptp error code.
*/
int close_os_if(struct os_ctx *ctx);

/** 
* Because the timestamp is implemented with 64 bit seconds field,
* the following macro is used to get 10-byte representation of the 
* timestamp in network byte order.
* @param time PTP timestamp
* @param buf buffer where to write timestamp.
*/
void ptp_format_timestamp(struct Timestamp *time, u8 * buf);

/** 
* Because the timestamp is implemented with 64 bit seconds field,
* the following macro is used to convert 10-byte representation of the 
* timestamp in network byte order to struct Timestamp.
* @param time PTP timestamp
* @param buf timestamp in 10-byte network-byte-order.
*/
void ptp_convert_timestamp(struct Timestamp *time, u8 * buf);

/** 
* Get random number. NOTE: this is best effort, no good randominess quaranteed.
* @param min minimum value for random number.
* @param max maximum value for random number.
* @return random number between requested values.
*/
int ptp_random(int min, int max);

/** 
* Return bigger value.
* @param v1 value 1.
* @param v2 value 2.
* @return bigger value.
*/
int ptp_max(int v1, int v2);

#endif                          //_OS_IF_H_
