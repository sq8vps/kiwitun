#ifndef ROUTE_H_
#define ROUTE_H_

#include "common.h"
#include <stdint-gcc.h>
#include <netinet/in.h>

/**
 * @brief Get tunnel IPv4 endpoint address for given IPv4 destination address
 * @return Tunnel endpoint address or 0 if not found
**/
in_addr_t Route_get(in_addr_t address);

/**
 * @brief Get and update local routing table
 * @return 0 on success, -1 on failure 
**/
int Route_update();

/**
 * @brief Initialize routing module
 * @return 0 on success, -1 on failure 
**/
int Route_init();

#endif