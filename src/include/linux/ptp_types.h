/** @file ptp_types.h
* PTP types as Linux types.
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
#ifndef _PTP_TYPES_H_
#define _PTP_TYPES_H_

/// Types declared
typedef unsigned long long u64; ///< unsigned 64 bit number
typedef long long s64;          ///< signed 64 bit number
typedef unsigned long long u48; ///< unsigned 48 bit number
typedef long long s48;          ///< signed 48 bit number
typedef unsigned int u32;       ///< unsigned 32 bit number
typedef int s32;                ///< signed 32 bit number
typedef unsigned short u16;     ///< unsigned 16 bit number
typedef short s16;              ///< signed 16 bit number
typedef unsigned char u8;       ///< unsigned 8 bit number
typedef char s8;                ///< signed 8 bit number
typedef unsigned char bool;     ///< boolean type

#define true  1
#define false 0

#endif                          // _PTP_TYPES_H_
