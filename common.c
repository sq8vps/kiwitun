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



struct Config_s config;

