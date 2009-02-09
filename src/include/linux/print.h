/** @file print.h
* PTP print support. 
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
#ifndef _PRINT_H_
#define _PRINT_H_

#include <clock_if.h>

void print_clock_status(unsigned long offset_sec, signed long offset_nsec,
                        signed long drift, signed long long adjust,
                        signed long long adjust_P,
                        signed long long adjust_I);

void print_clock_state(enum ptp_event_clk event);

#endif                          // _PRINT_H_
