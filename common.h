/*
    This file is part of kiwitun.

    Kiwitun is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Kiwitun is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with kiwitun.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file common.h
 * @brief Common and uncategorized defines, macros, variables and functions. Configuration structure is defined here.
*/

#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

#define IP_MAX_PACKET_SIZE 65535
#define IPV4_HEADER_SIZE 20
#define IPV4_HEADER_CHECKSUM_POS 10

#define IPV4_HEADER_PROTO_IPIP 0x04
#define IPV4_HEADER_PROTO_IP6IP 0x29
#define IPV4_HEADER_PROTO_ICMP 0x01

#define IPVX_HEADER_VERSION_4 0x04
#define IPVX_HEADER_VERSION_6 0x06

#define IPV6_HEADER_SIZE 40

#define IPV6_HEADER_PROTO_ICMP6 0x3A

#define ICMP_HEADER_SIZE 8
#define ICMP_ADDITIONAL_DATA_SIZE 8
#define ICMP_DEFAULT_TTL 64
#define ICMP_HEADER_CHECKSUM_POS 2


#define DEFAULT_HOSTNAME_REFRESH 60 //default hostname refresh time in minutes

#define KIWITUN_VERSION_STRING "kiwitun v. 1.0.0\nAn open-source module-independent tunneling engine\nLicensed under GNU GPL 3.0.\nhttps://github.com/sq8vps/kiwitun\n"

struct Config_s
{
    uint8_t debug : 1; //debug (verbose) mode enabled
    uint8_t tun4in4 : 1; //enable IPIP (4-in-4) tunneling
    uint8_t tun6in4 : 1; //enable IP6IP (6-in-4) tunneling
    uint8_t noDaemon : 1; //do not start as a daemon
    uint8_t ttl; //TTL/hop limit value for outer IP header
    struct in_addr local, remote; //local and remote IPv4 address (INADDR_ANY/NULL for automatic selection)
    struct in6_addr local6, remote6; //local and remote IPv6 address (inaddr6_any for automatic selection)
    char *hostname; //hostname as a remote address
    uint32_t hostnameRefresh; //hostname refresh interval in minutes
    char *ifName; //interface name
    uint8_t logLevel; //logging level (Syslog values)
};

extern struct Config_s config;

/**
 * @brief IPv6 pseudo-header for checksum calculation 
**/
struct ip6_pseudohdr
{
    struct in6_addr ip6_src; //source address
    struct in6_addr ip6_dst; //destination address
    uint32_t ip6_len; //payload length
    uint8_t zeros[3]; //must be zeroed
    uint8_t ip6_next; //protocol type
};

#define PRINT(level, ...) {\
    if(config.noDaemon && (level <= config.logLevel)){\
        printf(__VA_ARGS__);}\
    else{\
        syslog(level, __VA_ARGS__);}\
}

#define DEBUG(level, arg) {\
    if(config.noDaemon && (level <= config.logLevel)){\
        perror(arg);}\
    else{\
        syslog(level, "%s: %s\n", arg, strerror(errno));}\
}

#define DEFAULT_IPV4_TTL 64 //default TTL for IPv4 encapsulated packets

/**
 * @brief Get IPv4 address from string and store it in a structure
 * @param s Structure to store the address
 * @param addr Address string
 * @return 0 on success, -1 on failure
**/
int setAddress(struct in_addr *s, char *addr);

/**
 * @brief Get IPv6 address from string and store it in a structure
 * @param s Structure to store the address
 * @param addr Address string
 * @return 0 on success, -1 on failure
**/
int setAddress6(struct in6_addr *s, char *addr);

/**
 * @brief Print IPv4 address
 * @param logLevel Logging level
 * @param addr Address structure
**/
void printAddress(int logLevel, struct in_addr *addr);

/**
 * @brief Print IPv6 address
 * @param logLevel Logging level
 * @param addr Address structure
**/
void printAddress6(int logLevel, struct in6_addr *addr);

/**
 * @brief Check if two IPv6 addresses are equal (fast)
 * @param a1 Address 1
 * @param a2 Address 2
 * @return 1 if equal, 0 if not 
**/
int ipv6_isEqual(struct in6_addr a1, struct in6_addr a2);

/**
 * @brief Compare two IPv6 addresses
 * @param a1 Address 1
 * @param a2 Address 2
 * @return 1 if a1 > a2, -1 if a2 > a1, 0 if a1 = a2
**/
int ipv6_compare(struct in6_addr a1, struct in6_addr a2);

/**
 * @brief Perform bitwise AND on two IPv6 addresses
 * @param a1 Address 1
 * @param a2 Address 2
 * @return Bitwise AND of a1 and a2 (a1 & a2) 
**/
struct in6_addr ipv6_and(struct in6_addr a1, struct in6_addr a2);


/**
 * @brief Parse input arguments and store them in configuration structure
 * @param argc Argument count
 * @param argv Arguments 
 * @return 0 on success, -1 on failure (program must be terminated)
**/
int parseArgs(int argc, char **argv);

#endif