/** @file xml_parser.h
* XML parser declarations, used for reading PTP config.
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
#ifndef _XML_PARSER_H_
#define _XML_PARSER_H_

// Negative parser error codes
#define PARSER_OK               0
#define PARSER_ERR_NOT_FOUND    -1
#define PARSER_ERR_VALUE_LEN    -5

// Function declarations

/**
* Search specified tag. Used to find start of section.
* @param fp file pointer.
* @param tag tag to find from file.
* @param section_length Length of the section where subsection can be searched.
* @return parser error code or length of the section.
*/
int search_tag(FILE * fp, const char *tag, int section_length);

/**
* Search specified tag with attribute. Used to find start of section.
*
* @param fp file pointer.
* @param tag tag to find from file.
* @param attr store possible attribute here.
* @param attr_len maximum length for attribute.
* @param attr_value store possible attribute value here.
* @param attr_value_len maximum length for attribute value.
* @return parameter length or negative parser XML_ERROR code.
*/
int search_tag_with_attr(FILE * fp, const char *tag,
                         char *attr, int attr_len,
                         char *attr_value, int attr_value_len,
                         int *section_length);

/**
* Parse specific value and return it as a string.
* @param fp file pointer.
* @param tag tag to find from file.
* @param value buffer to where write the value
* @param value_len value buffer length
* @param section_length On input, length of the section available, on output section length after the value.
* @return parser error code.
*/
int parse_str(FILE * fp, const char *tag, char *value, int value_len,
              int *section_length);


/**
* Parse specific value and return it as an integer.
* @param fp file pointer.
* @param tag tag to find from file.
* @param value integer to where write the value
* @param section_length On input, length of the section available, on output section length after the value.
* @return parser error code.
*/
int parse_int(FILE * fp, const char *tag, int *value, int *section_length);

#endif                          // _XML_PARSER_H_
