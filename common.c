#include "common.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <arpa/inet.h>

int setAddress(struct in_addr *s, char *addr)
{
    if(inet_pton(AF_INET, addr, s) < 1) //convert
    {
        printf("%s is not a correct IPv4 address\n", addr);
        return -1;
    }
    return 0;
}

int setAddress6(struct in6_addr *s, char *addr)
{
    if(inet_pton(AF_INET6, addr, s) < 1) //convert
    {
        printf("%s is not a correct IPv6 address\n", addr);
        return -1;
    }
    return 0;
}

inline int ipv6_isEqual(struct in6_addr a1, struct in6_addr a2)
{
    return ((a1.__in6_u.__u6_addr32[0] == a2.__in6_u.__u6_addr32[0]) &
    (a1.__in6_u.__u6_addr32[1] == a2.__in6_u.__u6_addr32[1]) &
    (a1.__in6_u.__u6_addr32[2] == a2.__in6_u.__u6_addr32[2]) &
    (a1.__in6_u.__u6_addr32[3] == a2.__in6_u.__u6_addr32[3]) 
    );
}

int ipv6_compare(struct in6_addr a1, struct in6_addr a2)
{
    int16_t diff[16];
    //IPv6 addresses are stored MSByte first
    //if current byte of address 1 is bigger than of address 2, then address 1 must be bigger, etc.
    for(uint8_t i = 0; i < 16; i++)
    {
        diff[i] = (int16_t)a1.__in6_u.__u6_addr8[i] - (int16_t)a2.__in6_u.__u6_addr8[i]; //calculate difference for each byte
        if(diff[i] < 0) //non-zero difference, this byte is bigger in a2, so whole address is bigger
            return -1;
        else if(diff[i] > 0)
            return 1;
    }

    return 0; //all bytes were equal - both addresses are equal

}

inline struct in6_addr ipv6_and(struct in6_addr a1, struct in6_addr a2)
{
    struct in6_addr ret;
    for(uint8_t i = 0; i < 4; i++)
        ret.__in6_u.__u6_addr32[i] = a1.__in6_u.__u6_addr32[i] & a1.__in6_u.__u6_addr32[i];

    return ret;
}

struct Config_s config;

