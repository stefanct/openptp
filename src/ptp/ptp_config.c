/** @file ptp_config.c
* PTP configuration handling.
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
#include <stdio.h>
#include <ptp_general.h>
#include <xml_parser.h>

/// Main configuration data
struct ptp_config ptp_cfg;

// Helper tables for handling configuration
struct ClockAccuracyCmp str_to_accuracy[] = {
    {"25ns", CLOCK_25NS},
    {"100ns", CLOCK_100NS},
    {"250ns", CLOCK_250NS},
    {"1us", CLOCK_1US},
    {"2,5us", CLOCK_2_5US},
    {"10us", CLOCK_10US},
    {"25us", CLOCK_25US},
    {"100us", CLOCK_100US},
    {"250us", CLOCK_250US},
    {"1ms", CLOCK_1MS},
    {"2,5ms", CLOCK_2_5MS},
    {"10ms", CLOCK_10MS},
    {"25ms", CLOCK_25MS},
    {"100ms", CLOCK_100MS},
    {"250ms", CLOCK_250MS},
};
const unsigned int str_to_accuracy_size =
    sizeof(str_to_accuracy) / sizeof(struct ClockAccuracyCmp);

struct TimeSourceCmp str_to_source[] = {
    {"atomic clock", TS_ATOMIC_CLOCK},
    {"gps", TS_GPS},
    {"terrestrial radio", TS_TERRESTRIAL_RADIO},
    {"ptp", TS_PTP},
    {"ntp", TS_NTP},
    {"hand set", TS_HAND_SET},
    {"other", TS_OTHER},
    {"internal oscillator", TS_INTERNAL_OSCILLATOR},
};
const unsigned int str_to_source_size =
    sizeof(str_to_source) / sizeof(struct TimeSourceCmp);

/**
* Read initialization from file.
* @param filename config file name.
* @return ptp error code
*/
int read_initialization(char *filename)
{
    FILE *fp = 0;
    char tmp[MAX_VALUE_LEN];
    int value = 0;
    int i = 0, ret = 0, j = 0;
    int section_length = 0;

    if (filename == NULL) {
        return PTP_ERR_GEN;
    }

    DEBUG("Open %s\n", filename);
    fp = fopen(filename, "r");
    if (fp <= 0) {
        perror("open");
        return PTP_ERR_GEN;
    }
    // Check version
    if (parse_str(fp, "config_ver", tmp, 20, &section_length) != PARSER_OK) {
        ERROR("config_ver not found\n");
        return PTP_ERR_GEN;
    }
    if (strncmp(tmp, CONFIG_VERSION, MAX_VALUE_LEN) != 0) {
        ERROR("config version %s not supported\n", tmp);
        return PTP_ERR_GEN;
    }
    // Start by parsing general options
    fseek(fp, 0, SEEK_SET);
    section_length = search_tag(fp, "General", 0);
    if (section_length < PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    // get debug flag
    if (parse_int(fp, "debug", &value, &section_length) != PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("debug %i\n", value);
    ptp_cfg.debug = value;

    // get custom_clk_if flag
    if (parse_int(fp, "custom_clk_if", &value, &section_length) !=
        PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("custom_clk_if %i\n", value);
    ptp_cfg.custom_clk_if = value;

    // get clock_status_file flag
    if (parse_int(fp, "clock_status_file", &value, &section_length) !=
        PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("clock_status_file %i\n", value);
    ptp_cfg.clock_status_file = value;

    // Start parsing Interfaces
    fseek(fp, 0, SEEK_SET);
    ptp_cfg.num_interfaces = 0;
    for (i = 0; i < MAX_NUM_INTERFACES; i++) {
        // Get next interface 
        ret = search_tag_with_attr(fp, "Interface",
                                   tmp, MAX_VALUE_LEN,
                                   ptp_cfg.interfaces[ptp_cfg.
                                                      num_interfaces].name,
                                   INTERFACE_NAME_LEN, 0);
        if (ret < PARSER_OK) {
            DEBUG("No more Interfaces\n");
            break;
        }

        if (strncmp(tmp, "name", MAX_VALUE_LEN) != 0) {
            ERROR("Unknown attribute for Interface %s\n", tmp);
        }
        ptp_cfg.interfaces[ptp_cfg.num_interfaces].enabled = 1;

        DEBUG("%s=\"%s\"\n",
              tmp, ptp_cfg.interfaces[ptp_cfg.num_interfaces].name);
        // multicast setting
        ret = parse_int(fp, "multicast", &value, &section_length);
        if ((ret != PARSER_OK) || (value == 0)) {
            ptp_cfg.interfaces[ptp_cfg.num_interfaces].multicast_ena = 0;
        } else {
            ptp_cfg.interfaces[ptp_cfg.num_interfaces].multicast_ena = 1;
        }
        ptp_cfg.interfaces[ptp_cfg.num_interfaces].num_unicast_addr = 0;
        for (j = 0; j < MAX_NUM_INTERFACES; j++) {
            // get IP
            if (parse_str(fp, "unicast", tmp, MAX_VALUE_LEN,
                          &section_length) != PARSER_OK) {
                break;
            }
            ptp_cfg.interfaces[ptp_cfg.num_interfaces].num_unicast_addr++;
            strncpy(ptp_cfg.interfaces[ptp_cfg.num_interfaces].
                    unicast_ip[j], tmp, IP_STR_MAX_LEN);
        }

        DEBUG("Interface found %i(%s): mult: %i, unicast: %i\n",
              i,
              ptp_cfg.interfaces[ptp_cfg.num_interfaces].name,
              ptp_cfg.interfaces[ptp_cfg.num_interfaces].multicast_ena,
              ptp_cfg.interfaces[ptp_cfg.num_interfaces].num_unicast_addr);
        for (j = 0;
             j <
             ptp_cfg.interfaces[ptp_cfg.num_interfaces].num_unicast_addr;
             j++) {
            DEBUG("IP %s\n",
                  ptp_cfg.interfaces[ptp_cfg.num_interfaces].
                  unicast_ip[j]);
        }
        ptp_cfg.num_interfaces++;
    }
    // Start parsing Basic options
    fseek(fp, 0, SEEK_SET);
    section_length = search_tag(fp, "Basic", 0);
    if (section_length < PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    // get one_step_clock flag
    if (parse_int(fp, "one_step_clock", &value, &section_length) !=
        PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("one_step_clock %i\n", value);
    ptp_cfg.one_step_clock = value;

    // Start parsing Clock options
    fseek(fp, 0, SEEK_SET);
    section_length = search_tag(fp, "Clock", 0);
    if (section_length < PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    // get clock_class
    if (parse_int(fp, "clock_class", &value, &section_length) != PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("clock_class %i\n", value);
    ptp_cfg.clock_class = value;

    // get clock_accuracy
    if (parse_str
        (fp, "clock_accuracy", tmp, MAX_VALUE_LEN,
         &section_length) != PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    for (i = 0; i < str_to_accuracy_size; i++) {
        if (strncmp(tmp, str_to_accuracy[i].str, MAX_VALUE_LEN) == 0) {
            DEBUG("clock_accuracy 0x%x\n", str_to_accuracy[i].number);
            ptp_cfg.clock_accuracy = str_to_accuracy[i].number;
            break;
        }
    }
    if (i == str_to_accuracy_size) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    // get clock_priority1
    if (parse_int(fp, "clock_priority1", &value, &section_length) !=
        PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("clock_priority1 %i\n", value);
    ptp_cfg.clock_priority1 = value;

    // get clock_priority2
    if (parse_int(fp, "clock_priority2", &value, &section_length) !=
        PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("clock_priority2 %i\n", value);
    ptp_cfg.clock_priority2 = value;

    // get domain
    if (parse_int(fp, "domain", &value, &section_length) != PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("domain %i\n", value);
    ptp_cfg.domain = value;

    // get clock_source
    if (parse_str(fp, "clock_source", tmp, MAX_VALUE_LEN, &section_length)
        != PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    for (i = 0; i < str_to_source_size; i++) {
        if (strncmp(tmp, str_to_source[i].str, MAX_VALUE_LEN) == 0) {
            DEBUG("clock_source 0x%x\n", str_to_source[i].time_src);
            ptp_cfg.clock_source = str_to_source[i].time_src;
            break;
        }
    }
    if (i == str_to_source_size) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    // Start parsing Intervals options
    fseek(fp, 0, SEEK_SET);
    section_length = search_tag(fp, "Intervals", 0);
    if (section_length < PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    // get announce_interval
    if (parse_int(fp, "announce_interval", &value, &section_length) !=
        PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("announce_interval %i\n", value);
    ptp_cfg.announce_interval = value;

    // get sync_interval
    if (parse_int(fp, "sync_interval", &value, &section_length) !=
        PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("sync_interval %i\n", value);
    ptp_cfg.sync_interval = value;

    // get delay_req_interval
    if (parse_int(fp, "delay_req_interval", &value, &section_length) !=
        PARSER_OK) {
        ERROR("parse\n");
        return PTP_ERR_GEN;
    }
    DEBUG("delay_req_interval %i\n", value);
    ptp_cfg.delay_req_interval = value;

    fclose(fp);

    return PTP_ERR_OK;
}
