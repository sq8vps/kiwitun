#ifndef ROUTE_H_
#define ROUTE_H_

#include "common.h"
#include <stdint.h>
#include <netinet/in.h>

/**
 * @brief Get tunnel IPv4 endpoint address for given IPv4 destination address
 * @return Tunnel endpoint address or 0 (INADDR_ANY) if not found
**/
in_addr_t Route_get(in_addr_t address);

/**
 * @brief Get tunnel IPv6 endpoint address for given IPv6 destination address
 * @return Tunnel endpoint address or in6addr_any (all zeros) if not found
**/
struct in6_addr Route_get6(struct in6_addr address);

/**
 * @brief Unmap IPv4-mapped IPv6 address
 * @param address IPv6 address with IPv4 address inside
 * @return Unampped IPv4 address or 0 (INADDR_ANY) if this is not a IPv4-mapped IPv6 address 
**/
in_addr_t Route_unmap(struct in6_addr address);

/**
 * @brief Initialize routing module, get all available routes and start listening for route changes
 * @return 0 on success, -1 on failure 
**/
int Route_init();

/**
 * @brief Print routing table (IPv4 and IPv6) 
**/
void Route_print();

#endif