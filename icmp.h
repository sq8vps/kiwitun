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
 * @warning Sanity check of input IPv4 packet is the responsibility of caller.
**/
int ICMP_send(int s, uint8_t *data, int size, in_addr_t source, uint8_t type, uint8_t code, uint32_t rest);

#endif