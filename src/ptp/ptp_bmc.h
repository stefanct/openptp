/** @file ptp_bmc.h
* PTP Best master selection header. 
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
#ifndef _PTP_BMC_H_
#define _PTP_BMC_H_

#include <ptp_general.h>
#include <ptp_config.h>

/**
* PTP best master selection algorithm.
* @param ptp_ctx main context.
*/
void ptp_bmc_run(struct ptp_ctx *ptp_ctx);

#endif                          // _PTP_BMC_H_
