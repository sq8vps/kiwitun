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
 * @file icmp.h
 * @brief ICMP module
 * 
 * Routines for sending ICMP and ICMPv6 packets.
*/

#ifndef ICMP_H_
#define ICMP_H_

#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/in.h>
#include "common.h"

/**
 * @brief Send ICMP(v4) response for given IPv4 packet 
 * @param s Raw IP (!) socket handler
 * @param data Buffer containing IPv4 packet with header to refer to in the ICMP response
 * @param size Buffer size
 * @param source Source address or 0 (IN_ADDR_ANY) for auto selection
 * @param type ICMP response type
 * @param code ICMP response code
 * @param rest ICMP respose rest of header (or 0 if not used)
 * @return 0 on success, -1 on failure
 * @warning Sanity check of input IPv4 packet is the responsibility of a caller.
**/
int ICMP_send(int s, uint8_t *data, int size, in_addr_t source, uint8_t type, uint8_t code, uint32_t rest);

/**
 * @brief Send ICMP(v6) response for given IPv6 packet 
 * @param s Raw IP (!) socket handler
 * @param data Buffer containing IPv6 packet with header to refer to in the ICMP response
 * @param size Buffer size
 * @param source Source address or in6addr_any for auto selection
 * @param type ICMP response type
 * @param code ICMP response code
 * @param rest ICMP respose rest of header (or 0 if not used)
 * @return 0 on success, -1 on failure
 * @warning Sanity check of input IPv6 packet is the responsibility of a caller.
**/
int ICMP_send6(int s, uint8_t *data, int size, struct in6_addr source, uint8_t type, uint8_t code, uint32_t rest);

#endif