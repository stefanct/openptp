/** @file clock_if.h
* Clock interface description.
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

#ifndef _CLOCK_IF_H_
#define _CLOCK_IF_H_
#include "ptp_general.h"

/**
* Clock context information.
*/
struct clock_ctx {
    void *arg;                  ///< private data for clock specific data    
};

/**
* Enum for PTP event reporting to clock module.
*/
enum ptp_event_clk {
    PTP_MASTER_CHANGED,         ///< master clock has changed (device is slave)
    PTP_CLK_MASTER,             ///< device becomes master
};

/**
* Enum for PTP event reporting to PTP module.
*/
enum ptp_event_ctrl {
    PTP_MASTER_CLOCK_SELECTED,  ///< master clock has been updated
};

/** 
* These API functions are called by PTP module and implemented by 
* clock module. 
*/

/**
* Function for initializing clock interface. 
* @param ctx clock context
* @param cfg_file Clock interface configuration file name
* @return ptp error code.
*/
int ptp_initialize_clock_if(struct clock_ctx *ctx, char* cfg_file);

/**
* Function for reconfiguring clock interface. 
* @param ctx clock context
* @param cfg_file Clock interface configuration file name
* @return ptp error code.
*/
int ptp_reconfig_clock_if(struct clock_ctx *ctx, char* cfg_file);

/**
* Function for closing clock interface. 
* @param ctx clock context
* @return ptp error code.
*/
int ptp_close_clock_if(struct clock_ctx *ctx);

/**
* An event reporting function to clock module.
* @param ctx clock context
* @param event status value indication function call reason.
* @param arg event specific argument.
*/
void ptp_event_clk(struct clock_ctx *ctx,
                   enum ptp_event_clk event, void *arg);

/**
* Function for retrieving current time. 
* @param ctx clock context
* @param time time in ptp format.
* @return ptp error code.
*/
int ptp_get_time(struct clock_ctx *ctx, struct Timestamp *time);

/**
* Function for retrieving local clock properities. 
* @param ctx clock context
* @param time_dataset Time Properities dataset
* @return ptp error code.
*/
int ptp_get_clock_properities(struct clock_ctx *ctx,
                              struct TimeProperitiesDataSet *time_dataset);

/**
* Function for reporting received sync and follow_up timestamps. 
* @param ctx clock context
* @param master_time master timestamp.
* @param slave_time slave timestamp.
*/
void ptp_sync_rcv(struct clock_ctx *ctx,
                  struct Timestamp *master_time,
                  struct Timestamp *slave_time);

/**
* Function for reporting received delay request timestamps. 
* @param ctx clock context
* @param slave_time slave timestamp.
* @param master_time master timestamp.
*/
void ptp_delay_rcv(struct clock_ctx *ctx,
                   struct Timestamp *slave_time,
                   struct Timestamp *master_time);


/** 
* These API functions are called by Clock module and implemented by 
* PTP module. 
*/

/**
* An event reporting function to main control.
* @param ctx clock context
* @param event status value indication function call reason.
* @param arg event specific argument.
*/
void ptp_event_ctrl(enum ptp_event_ctrl event, void *arg);

#endif                          // _CLOCK_IF_H_
