/** @file ptp_general.h
* PTP general definitions, datasets, enums, etc.
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
#ifndef _PTP_GENERAL_H_
#define _PTP_GENERAL_H_
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "linux/ptp_types.h"

/// Error codes.
#define PTP_ERR_OK       0      ///< No error
#define PTP_ERR_GEN     -1      ///< General error
#define PTP_ERR_FRAME   -2      ///< Frame error
#define PTP_ERR_TIMEOUT -3      ///< Timeout error
#define PTP_ERR_NET     -4      ///< Error from network interface

/**
* PTP protocol state machine states.
*/
enum PortState {
    PORT_INITIALIZING,
    PORT_FAULTY,
    PORT_DISABLED,
    PORT_LISTENING,
    PORT_PRE_MASTER,
    PORT_MASTER,
    PORT_PASSIVE,
    PORT_UNCALIBRATED,
    PORT_SLAVE,
};

/**
* BMC inputs for port state machine.
*/
enum BMCUpdate {
    BMC_MASTER_M1,
    BMC_MASTER_M2,
    BMC_MASTER_M3,
    BMC_PASSIVE_P1,
    BMC_PASSIVE_P2,
    BMC_SLAVE_S1,
};


/// PTP types.
/**
* The TimeInterval type represents time intervals.
*/
struct TimeInterval {
    s64 scaled_nanoseconds;     ///< nanoseconds multiplied by 2^16.
};

/**
* The Timestamp type represents a positive time with respect to the epoch.
*/
struct Timestamp {
    u48 seconds;                ///< portion of the timestamp in units of seconds.
    u32 nanoseconds;            ///< nanoseconds portion of the timestamp (<10^9).
    u16 frac_nanoseconds;       ///< fractional nanoseconds part
};

/**
* The ClockIdentity type that identifies a PTP clock.
*/
typedef u8 ClockIdentity[8];

/**
* The PortIdentity type identifies a PTP port.
*/
struct PortIdentity {
    ClockIdentity clock_identity;
    u16 port_number;            ///< PTP port
};

/**
* Network protocol enumeration.
*/
enum NetworkProtocol {
    UDP_IPv4 = 1,
    UDP_IPv6 = 2,
    IEEE802_3 = 3,
    DEVICE_NET = 4,
    CONTROL_NET = 5,
    PROFINET = 6,
};

/**
* The PortAddress type represents the protocol address of a PTP port.
*/
struct PortAddress {
    enum NetworkProtocol network_protocol;
    u16 address_length;
    u8 address[16];
};

/**
* Clock accuracy enumeration.
*/
typedef u8 ClockAccuracy;
enum ClockAccuracy_enum {
    CLOCK_25NS = 0x20,          ///< The time is accurate to within 25 ns
    CLOCK_100NS = 0x21,         ///< The time is accurate to within 100 ns
    CLOCK_250NS = 0x22,         ///< The time is accurate to within 250 ns
    CLOCK_1US = 0x23,           ///< The time is accurate to within 1 us
    CLOCK_2_5US = 0x24,         ///< The time is accurate to within 2.5 us
    CLOCK_10US = 0x25,          ///< The time is accurate to within 10 us
    CLOCK_25US = 0x26,          ///< The time is accurate to within 25 us
    CLOCK_100US = 0x27,         ///< The time is accurate to within 100 us
    CLOCK_250US = 0x28,         ///< The time is accurate to within 250 us
    CLOCK_1MS = 0x29,           ///< The time is accurate to within 1 ms
    CLOCK_2_5MS = 0x2A,         ///< The time is accurate to within 2.5 ms
    CLOCK_10MS = 0x2B,          ///< The time is accurate to within 10 ms
    CLOCK_25MS = 0x2C,          ///< The time is accurate to within 25 ms
    CLOCK_100MS = 0x2D,         ///< The time is accurate to within 100 ms
    CLOCK_250MS = 0x2E,         ///< The time is accurate to within 250 ms
    CLOCK_1S = 0x2F,            ///< The time is accurate to within 1 s
    CLOCK_10S = 0x30,           ///< The time is accurate to within 10 s
    CLOCK_OVER10S = 0x31,       ///< The time is accurate to >10 s
    // 0x80-0xFD For use by alternate PTP profiles
};

/**
* The ClockQuality represents the quality of a clock.
*/
struct ClockQuality {
    u8 clock_class;
    ClockAccuracy clock_accuracy;
    u16 offset_scaled_log_variance;
};

/**
* TLV type enumeration.
*/
enum TlvType {
    MANAGEMENT = 0x0001,
    MANAGEMENT_ERROR_STATUS = 0x0002,
    VENDOR_EXTENSION = 0x0003,
    STANDARDS_ORGANIZATION_EXTENSION = 0x0004,
    REQUEST_UNICAST_TRANSMISSION = 0x0005,
    GRANT_UNICAST_TRANSMISSION = 0x0006,
    CANCEL_UNICAST_TRANSMISSION = 0x0007,
    ACKNOWLEDGE_CANCEL_UNICAST_TRANSMISSION = 0x0008,
    PATH_TRACE = 0x0009,
    ALTERNATE_TIME_OFFSET_INDICATOR = 0x000A,
    // Experimental
    AUTHENTICATION = 0x2000,
    AUTHENTICATION_CHALLENGE = 0x2001,
    SECURITY_ASSOCIATION_UPDATE = 0x2002,
    CUM_FREQ_SCALE_FACTOR_OFFSET = 0x2003,
};

/**
* The TLV type represents TLV extension fields. 
* The length of all TLVs shall be an even number of octets.
*/
struct TLV {
    enum TlvType tlv_type;
    u16 length;
    u8 value[];
};

/**
* The PTPText data type is used to represent textual material in PTP messages.
* The text field shall be encoded as UTF-8 symbols as specified by ISO/IEC 10646. 
* The most significant octet of the leading text symbol shall be 
* the element of the array with index 0.
*/
struct PTPText {
    u8 length;
    u8 text[];
};

/**
* Fault severity enumeration.
*/
enum FaultSeverity {
    FAULT_EMERGENCY = 0,        ///< Emergency: system is unusable
    FAULT_ALERT = 1,            ///< Alert: immediate action needed
    FAULT_CRITICAL = 2,         ///< Critical: critical conditions
    FAULT_ERROR = 3,            ///< Error: error conditions
    FAULT_WARNING = 4,          ///< Warning: warning conditions
    FAULT_NOTICE = 5,           ///< Notice: normal but significant condition
    FAULT_INFORMATIONAL = 6,    ///< Informational: informational messages
    FAULT_DEBUG = 7,            ///< Debug: debug-level messages
};

/**
* The FaultRecord type is used to construct fault logs.
*/
struct FaultRecord {
    u16 fault_record_length;
    struct Timestamp fault_time;
    enum FaultSeverity severity;
    struct PTPText fault_name;  ///< variable length
    struct PTPText fault_value; ///< variable length
    struct PTPText fault_description;   ///< variable length
};

/**
* Time source enumeration.
*/
enum TimeSource {
    TS_ATOMIC_CLOCK = 0x10,     ///< Any device, or device directly connected to atomic clock
    TS_GPS = 0x20,              ///< Any device synchronized to any of the satellite systems
    TS_TERRESTRIAL_RADIO = 0x30,        ///< Any device synchronized via any of the radio distribution systems
    TS_PTP = 0x40,              ///< PTP Any device synchronized to a PTP based source
    TS_NTP = 0x50,              ///< Any device synchronized via NTP to servers
    TS_HAND_SET = 0x60,         ///< Used in all cases for any device whose time has been set human interface based on observation
    TS_OTHER = 0x90,            ///< Other source of time and/or frequency not covered by other values
    TS_INTERNAL_OSCILLATOR = 0xA0,      ///< Any device whose time is based on a free-running non-calibrated oscillator
};

/**
* Delay mechanism enumeration.
*/
enum DelayMechanism {
    DELAY_E2E = 0x01,           ///< The port is configured to use the delay request-response mechanism.
    DELAY_P2P = 0x02,           ///< The port is configured to use the peer delay mechanism.    
    DELAY_DISABLED = 0xFE,      ///< The port does not implement the delay mechanism.
};

/// PT domains
#define DEFAULT_DOMAIN      0   ///< Default domain
#define ALTERNATE_DOMAIN1   1   ///< Alternate domain 1
#define ALTERNATE_DOMAIN2   2   ///< Alternate domain 2
#define ALTERNATE_DOMAIN3   3   ///< Alternate domain 3
// 4-127 User defined Domains

/// Datasets.
/**
* Default dataset.
*/
struct DefaultDataSet {
    bool two_step_clock;        ///< TRUE for two_step clock
    ClockIdentity clock_identity;       ///< Local clock identity
    u32 num_ports;              ///< number of ports, 1 for Ordinary clock       
    struct ClockQuality clock_quality;  ///< quality of the clock
    u8 priority1;               ///< Priority1 
    u8 priority2;               ///< Priority2 
    u8 domain;                  /// PTP domain
    bool slave_only;            ///< true if only slave
    bool security_enabled;      ///< TRUE for security enabled PTP
    u32 number_security_associations;   ///< number of security associations supported
};

/**
* Current dataset.
*/
struct CurrentDataSet {
    u32 steps_removed;          ///< The number of communication paths traversed between the local clock and the grandmaster
    struct TimeInterval offset_from_master;     ///< The time difference between a master and a slave as computed by the slave
    struct TimeInterval mean_path_delay;        ///< The mean propagation time between a master and slave as computed by the slave  
};

/**
* Parent dataset.
*/
struct ParentDataSet {
    struct PortIdentity parent_port_identity;   ///< The master port that issues the Sync messages used in synchronizing this clock
    bool parent_stats;          ///< TRUE if the clock has a port in the SLAVE state and observed_parent_offset_scaled_log_variance and observed_parent_clock_phase_change_rate are valid.
    s16 observed_parent_offset_scaled_log_variance;     ///< 
    s32 observed_parent_clock_phase_change_rate;        ///< 
    ClockIdentity grandmaster_identity; ///< Grandmaster identity
    struct ClockQuality grandmaster_clock_quality;      ///< Grandmasterc clock quality
    u8 grandmaster_priority1;   ///< Grandmaster priority1 
    u8 grandmaster_priority2;   ///< Grandmaster priority2 
};

/**
* Time properities dataset.
*/
struct TimeProperitiesDataSet {
    u16 current_utc_offset;     ///< The offset between TAI and UTC
    bool current_utc_offset_valid;      ///< TRUE if current_utc_offset is valid
    bool leap_59;               ///< TRUE if the last minute of the current UTC day will contain 59 seconds.
    bool leap_61;               ///< TRUE if the last minute of the current UTC day will contain 61 seconds.
    bool time_traceable;        ///< TRUE if the timescale and the value of current_utc_offset are traceable to a primary standard.
    bool frequency_traceable;   ///< TRUE if the frequency determining the timescale is traceable to a primary standard.
    bool ptp_timescale;         ///< TRUE if the clock timescale of the grandmaster clock is PTP.
    enum TimeSource time_source;        ///< The source of time used by the grandmaster clock.      
};

/**
* Port dataset.
*/
struct PortDataSet {
    u8 version_number;          ///< PTP version used in port
    enum PortState port_state;  ///< Current port state.
    struct PortIdentity port_identity;  ///< Port identity of the port.
    s8 log_mean_announce_interval;      ///< Announce interval.
    s8 log_mean_sync_interval;  ///< Sync interval for multicast messages.
    s8 log_min_mean_delay_req_interval; ///< The minimum mean Delay_Req interval.
    s8 log_min_mean_pdelay_req_interval;        ///< Pdelay_Req interval.
    u8 announce_receipt_timeout;        ///< An integral multiple of Announce intervals.
    struct TimeInterval peer_mean_path_delay;   ///< an estimate of the current one-way propagation delay on the link attached to this port computed using the peer delay mechanism.
    enum DelayMechanism delay_mechanism;        ///< The propagation delay measuring option used by the port in computing mean_path_delay.
};

/**
* Security algorith.
*/
enum SecAlgo {
    PTP_SEC_ALGO_NULL = 0,
    PTP_SEC_ALGO_HMAC_SHA1_96 = 1,
};
#define MAX_KEY_LENGTH 32

/**
* SA trust state.
*/
enum TrustState {
    SA_UNTRUSTED = 0,
    SA_TRUSTED = 1,
};

/**
* SA challenge state.
*/
enum ChallState {
    SA_CHALL_IDLE = 0,
    SA_CHALLENGING = 1,
};

/**
* SA type.
*/
enum SAType {
    SA_STATIC = 0,
    SA_DYNAMIC = 1,
};

/**
* Security Key.
*/
struct SecKey {
    struct SecKey *next;
    u16 keyid;
    bool valid;
    enum SecAlgo algorithmid;
    u8 security_key[MAX_KEY_LENGTH];
    struct Timestamp start_time;
    struct Timestamp expiration_time;
};

/**
* Security Association (SA).
*/
#define MAX_ADDR_LEN 16
struct SecAssociation {
    struct SecAssociation *next;
    struct PortIdentity src_port;
    u8 src_addr[MAX_ADDR_LEN];
    struct PortIdentity dst_port;
    u8 dst_addr[MAX_ADDR_LEN];
    u32 replay_counter;
    u16 lifetime_id;
    u16 keyid;
    u16 next_lifetime_id;
    u16 next_keyid;
    enum TrustState trust_state;
    u16 trust_timer;
    enum ChallState chall_state;
    u16 chall_timer;
    u16 chall_timeout;
    u32 request_nonce;
    u32 responce_nonce;
    bool challenge_required;
    bool response_required;
    enum SAType type;
};

/**
* Security dataset.
*/
struct SecurityDataSet {
    u32 number_sec_keys;
    struct SecKey *sec_key_list;
    u32 number_sa_in_keys;
    struct SecAssociation *sa_in_list;
    u32 number_sa_out_keys;
    struct SecAssociation *sa_out_list;
};

#include <ptp_debug.h>
#include <ptp_config.h>

#endif                          // _PTP_GENERAL_H_
