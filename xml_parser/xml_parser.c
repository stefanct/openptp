/** @file xml_parser.c
* XML parser functions, used for reading PTP config.
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
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <values.h>

#include "xml_parser.h"

#define MAX_TAG_LENGTH          100

// debug macros
#define XML_ERROR(x...) { printf("ERROR in %s[%i]: ",__FUNCTION__, __LINE__); printf(x);}
#if 0
#define XML_DEBUG(x...) { printf("%s[%i]: ",__FUNCTION__, __LINE__); printf(x); }
#define XML_DEBUG_PLAIN(x...) { printf(x); }
#else
#define XML_DEBUG(x...)
#define XML_DEBUG_PLAIN(x...)
#endif

static int search_tag_position(FILE * fp, const char *tag,
                               long *tag_start_pos);


/**
* Search specified tag. Used to find start of section.
* @param fp file pointer.
* @param tag tag to find from file.
* @return parser XML_ERROR code or length of the section.
*/
int search_tag(FILE * fp, const char *tag, int section_length)
{
    int ret = 0;
    char tag_tmp[MAX_TAG_LENGTH];
    long start_section = 0, end_section = 0;
    long start_pos = ftell(fp);
    int sec_len = 0;

    XML_DEBUG("find <%s>\n", tag);

    ret = search_tag_position(fp, tag, &start_section);
    if (ret != PARSER_OK) {
        XML_DEBUG("Start tag <%s> not found\n", tag);
        goto error_out;
    }
    if (section_length && (ftell(fp) > start_pos + section_length)) {
        XML_DEBUG("Tag not found from current section\n");
        ret = PARSER_ERR_NOT_FOUND;
        goto error_out;
    }

    tag_tmp[0] = '/';
    strncpy(&tag_tmp[1], tag, MAX_TAG_LENGTH - 1);

    ret = search_tag_position(fp, tag_tmp, &end_section);
    if (ret != PARSER_OK) {
        XML_ERROR("End tag <%s> not found\n", tag);
        goto error_out;
    }
    if (section_length && (ftell(fp) > start_pos + section_length)) {
        XML_DEBUG("Tag not found from current section\n");
        ret = PARSER_ERR_NOT_FOUND;
        goto error_out;
    }

    sec_len = end_section - start_section;
    XML_DEBUG("Section length %i\n", sec_len);

    // rewind to the start of the section
    fseek(fp, start_section, SEEK_SET);

    return sec_len;

  error_out:
    // rewind to the start position
    fseek(fp, start_pos, SEEK_SET);
    return ret;
}

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
                         int *section_length)
{
    char c = 0;
    int i = 0;
    int ret = 0;
    int len = 0;

    XML_DEBUG("find <%s> with attibute len %i attribute value len %i\n",
              tag, attr_len, attr_value_len);

    len = section_length ? *section_length : 0;
    ret = search_tag(fp, tag, len);
    if (ret > 0) {
        for (i = 0; (c = getc(fp)) != EOF; i++) {
            // Attribute length check
            if (i >= attr_len) {
                return PARSER_ERR_NOT_FOUND;
            }
            // End of element check 
            if (c == '>') {
                // Element should contain an attribute -> error
                return PARSER_ERR_NOT_FOUND;
            }
            // End-of-attribute check (start of attribute value)
            if (c == '=') {
                c = getc(fp);
                // Atribute value in '"'
                if (c != '"') {
                    XML_ERROR("Attribute value not found\n");
                    return PARSER_ERR_NOT_FOUND;
                }
                break;          // Continue parsing attribute value
            }
            attr[i] = c;
        }
        attr[i] = 0;            // end-of-str (attribute) 

        for (i = 0; (c = getc(fp)) != EOF; i++) {
            // Attribute value length check
            if (i >= attr_value_len) {
                return PARSER_ERR_NOT_FOUND;
            }
            // End of element check 
            if (c == '>') {
                // Attribute must end to '"' -> error
                return PARSER_ERR_NOT_FOUND;
            }
            // End-of-attribute_value check 
            if (c == '"') {
                break;          // Done
            }
            attr_value[i] = c;
        }
        attr_value[i] = 0;      // end-of-str

        XML_DEBUG("tag with attribute <%s %s=\"%s\">\n",
                  tag, attr, attr_value);
        if (section_length) {
            *section_length = ret;
        }
    }

    if (section_length) {
        XML_DEBUG("Section length %i\n", *section_length);
    }

    return ret;
}


/**
* Parse specific value and return it as a string.
* @param fp file pointer.
* @param tag tag to find from file.
* @param value buffer to where write the value
* @param value_len value buffer length
* @return parser XML_ERROR code.
*/
int parse_str(FILE * fp, const char *tag, char *value, int value_len,
              int *section_length)
{
    char c = 0;
    int i = 0;
    int ret = 0;
    long start_pos = ftell(fp), end_pos = 0, tag_start_pos = 0;
    int parsed_length = 0;

    memset(value, 0, value_len);

    ret = search_tag_position(fp, tag, &tag_start_pos);
    if (ret != PARSER_OK) {
        goto error_out;
    }
    if (*section_length && (ftell(fp) > start_pos + *section_length)) {
        XML_DEBUG("Tag not found from current section\n");
        ret = PARSER_ERR_NOT_FOUND;
        goto error_out;
    }
    // found requested tag
    for (i = 0; (i < value_len) && ((c = getc(fp)) != EOF); i++) {
        if (c == '<') {
            break;
        }
        value[i] = c;
    }
    if (c != '<') {
        XML_ERROR("Value length overflow\n");
        return PARSER_ERR_VALUE_LEN;
    }
    XML_DEBUG("value: [%s]\n", value);

    // Check end tag (not needed:)   
    end_pos = ftell(fp);
    parsed_length = end_pos - start_pos;

    if (*section_length) {
        if (parsed_length > *section_length) {
            XML_DEBUG("Tag not found from current section\n");
            ret = PARSER_ERR_NOT_FOUND;
            goto error_out;
        }
        *section_length -= parsed_length;
        XML_DEBUG("Section length left %i (%i removed)\n",
                  *section_length, parsed_length);
    }

    return PARSER_OK;

  error_out:
    // rewind to the start position
    fseek(fp, start_pos, SEEK_SET);
    return ret;
}

/**
* Parse specific value and return it as an integer.
* @param fp file pointer.
* @param tag tag to find from file.
* @param value integer to where write the value
* @return parser XML_ERROR code.
*/
int parse_int(FILE * fp, const char *tag, int *value, int *section_length)
{
    char tmp[20];
    int ret = 0;

    ret = parse_str(fp, tag, tmp, 20, section_length);
    if (ret != PARSER_OK) {
        return ret;
    }

    *value = strtol(tmp, (char **) NULL, 0);

    if (*value == LONG_MIN) {
        XML_ERROR("Underflow\n");
        return PARSER_ERR_VALUE_LEN;
    }

    if (*value == LONG_MAX) {
        XML_ERROR("Overflow\n");
        return PARSER_ERR_VALUE_LEN;
    }

    XML_DEBUG("Value: %i\n", *value);

    return PARSER_OK;
}

/**
* Search specified tag position.
* @param fp file pointer.
* @param tag tag to find from file.
* @param tag_start_pos returns file position AFTER the tag.
* @return parser XML_ERROR code.
*/
static int search_tag_position(FILE * fp, const char *tag,
                               long *tag_start_pos)
{
    char c = 0;
    int i = 0;
    char tag_tmp[MAX_TAG_LENGTH];

    XML_DEBUG("find <%s>\n", tag);

    while ((c = getc(fp)) != EOF) {
        //XML_DEBUG_PLAIN("%c", c);
        if (c == '<') {
            // tag start
            for (i = 0;
                 (i < MAX_TAG_LENGTH - 1) && ((c = getc(fp)) != EOF);
                 i++) {
                if (c == '>') {
                    break;
                } else if (c == ' ') {
                    // tag can not contain space, instead tag parameter probably follows
                    break;
                }
                tag_tmp[i] = c;
            }
            tag_tmp[i] = 0;     // end-of-str

            XML_DEBUG("tag <%s>\n", tag_tmp);
            if (strncmp(tag, tag_tmp, MAX_TAG_LENGTH) == 0) {
                // found 
                XML_DEBUG("tag <%s> found\n", tag);
                *tag_start_pos = ftell(fp);
                return PARSER_OK;
            }
        }
    }

    return PARSER_ERR_NOT_FOUND;
}
