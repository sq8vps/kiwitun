#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include <stdint.h>
#include <netinet/ip.h>

struct Config_s
{
    uint8_t debug : 1; //debug (verbose) mode enabled

    uint8_t ttl; //TTL value for outer IP header
    struct in_addr local, remote; //local and remote IPv4 address (INADDR_ANY/NULL for automatic selection)
    struct in6_addr local6, remote6; //local and remote IPv6 address (inaddr6_any for )
};

extern struct Config_s config;

#define PRINT(...) {\
    if(config.debug)\
        printf(__VA_ARGS__);\
}

#define DEBUG(arg) {\
    if(config.debug)\
        perror(arg);\
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

#endif