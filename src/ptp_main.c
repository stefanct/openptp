/** @file ptp_main.c
* Main function.
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
#include <ptp_config.h>
#include <ptp_general.h>
#include "ptp/ptp.h"

/**
* Main function.
* @param argc Number of commandline parameters.
* @param argv Commandline parameters.
* @return Daemon return value.
*/
int main(int argc, char *argv[])
{

    ptp_main(argc, argv);

    return 0;
}
