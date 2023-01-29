/**
 * @file route.c
 * @brief Route selection module
 * 
 * This module maintains local routing tables acquired from kernel and provides a route lookup for given destination address.
 * It is shell- and system files-independent module that uses only netlink and rtnetlink Linux kernel functionalities.
 * Netlink and especially rtnetlink are VERY poorly documented. The code for reading routes from kernel is based
 * mainly on this answer: https://stackoverflow.com/a/3288983 (https://stackoverflow.com/questions/3288065/getting-gateway-to-use-for-a-given-ip-in-ansi-c)
**/

#include "route.h"
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define NETLINK_BUF_SIZE 16384 //buffer size for netlink messages
#define ROUTING_TABLE_BLOCK_SIZE 256 //number of entries in one routing table block (routing table is made of N routing table blocks)

/**
 * @brief Convert netmask in CIDR notation to IPv4 address
 * @param n CIDR netmask
 * @return Netmask address in network byte order
**/
#define CIDR_TO_ADDR4(n) htonl((in_addr_t)(~((1 << (32 - n)) - 1)))

/**
 * @brief Convert netmask in CIDR notation to IPv6 address
 * @param n CIDR netmask
 * @return Netmask address in network byte order
**/
struct in6_addr CIDR_TO_ADDR6(uint8_t n)
{
    if(n > 128)
        n = 128;
    struct in6_addr ret = {0};
    int i = 0;
    while(n > 0)
    {
        uint8_t k = (n > 8) ? 8 : n;
        ret.__in6_u.__u6_addr8[i] = (~((1 << (8 - k)) - 1)); //a compact way of filling byte with ones starting from MSB
        n -= k;
        i++;
    }
    return ret;
}

/**
 * @brief Structure of local IPv4 routing table entry 
**/
struct Route_s
{
    struct in_addr address;
    struct in_addr netmask;
    struct in_addr gateway;
};

/**
 * @brief Structure of local IPv6 routing table entry 
**/
struct Route6_s
{
    struct in6_addr address;
    struct in6_addr netmask;
    struct in6_addr gateway;
};

struct Route_s *routes = NULL; //local IPv4 routing table
uint16_t routeBlocks = 0; //number of reserved blocks for routing table
uint64_t routeEntries = 0; //number of entries in routing table

struct Route6_s *routes6 = NULL; //local IPv6 routing table
uint16_t route6Blocks = 0; //number of reserved blocks for routing table
uint64_t route6Entries = 0; //number of entries in routing table

/**
 * @brief Resize local IPv4 routing table
 * @param entryCount Entry count
 * @return 0 on success, -1 on failure
 * @attention If there is enough space already allocated will return with 0 immediately  
**/
int route_resizeTable(uint64_t entryCount)
{
    if(entryCount < (ROUTING_TABLE_BLOCK_SIZE * routeBlocks)) //still enough space, no need for reallocation
        return 0;
    
    //calculate count of needed blocks
    routeBlocks = (entryCount / ROUTING_TABLE_BLOCK_SIZE) + ((entryCount % ROUTING_TABLE_BLOCK_SIZE) ? 1 : 0);

    routes = realloc(routes, routeBlocks * ROUTING_TABLE_BLOCK_SIZE * sizeof(struct Route_s)); //realloc
    if(routes == NULL) //failure
    {
        PRINT("IPv4 routing table memory allocation failed\n");
        return -1;
    }

    return 0;
}

/**
 * @brief Resize local IPv6 routing table
 * @param entryCount Entry count
 * @return 0 on success, -1 on failure
 * @attention If there is enough space already allocated will return with 0 immediately  
**/
int route_resizeTable6(uint64_t entryCount)
{
    if(entryCount < (ROUTING_TABLE_BLOCK_SIZE * route6Blocks)) //still enough space, no need for reallocation
        return 0;
    
    //calculate count of needed blocks
    route6Blocks = (entryCount / ROUTING_TABLE_BLOCK_SIZE) + ((entryCount % ROUTING_TABLE_BLOCK_SIZE) ? 1 : 0);

    routes6 = realloc(routes6, route6Blocks * ROUTING_TABLE_BLOCK_SIZE * sizeof(struct Route6_s)); //realloc
    if(routes6 == NULL) //failure
    {
        PRINT("IPv6 routing table memory allocation failed\n");
        return -1;
    }

    return 0;
}

/**
 * @brief Parse route data and add to appropriate table
 * @param nl Netlink message 
**/
void route_parseAndAdd(struct nlmsghdr *nl)
{
    struct rtmsg *rt = (struct rtmsg *)NLMSG_DATA(nl); //get route message

    struct rtattr *rtAttr = (struct rtattr *)RTM_RTA(rt); //get first route attribute
    int len = RTM_PAYLOAD(nl);

    if(rt->rtm_family == AF_INET) //an IPv4 route
    {
        if(rt->rtm_type != RTN_UNICAST) //accept only gateways and direct routes
            return;

        route_resizeTable(routeEntries + 1); //add new route entry
        routes[routeEntries].netmask.s_addr = CIDR_TO_ADDR4(rt->rtm_dst_len); //get netmask length and convert it to address

        for (; RTA_OK(rtAttr, len); rtAttr = RTA_NEXT(rtAttr, len)) //go through all attributes
        {
            switch(rtAttr->rta_type) 
            {
                case RTA_GATEWAY: //get gateway
                    routes[routeEntries].gateway.s_addr = *(in_addr_t*)RTA_DATA(rtAttr);
                    break;
                case RTA_DST: //get destination
                    routes[routeEntries].address.s_addr = *(in_addr_t*)RTA_DATA(rtAttr);
                    break;
                default:
                    break;
            }
        }

        if(routes[routeEntries].address.s_addr != INADDR_ANY) //is this not a default gateway?
            routeEntries++;
        //omit default gateways
    }
    else if(rt->rtm_family == AF_INET6) //an IPv6 route
    {
        route_resizeTable6(route6Entries + 1); //add new route entry
        routes6[route6Entries].netmask = in6addr_any;
        routes6[route6Entries].netmask = CIDR_TO_ADDR6(rt->rtm_dst_len); //get netmask length and convert it to address

        for (; RTA_OK(rtAttr, len); rtAttr = RTA_NEXT(rtAttr, len)) //go through all attributes
        {
            if(rt->rtm_type != RTN_UNICAST) //accept only gateways or direct routes
                return;
            
            switch(rtAttr->rta_type) 
            {
                case RTA_GATEWAY: //get gateway
                    routes6[route6Entries].gateway = *(struct in6_addr*)RTA_DATA(rtAttr);
                    break;
                case RTA_DST: //get destination
                    routes6[route6Entries].address = *(struct in6_addr*)RTA_DATA(rtAttr);
                    break;
                default:
                    break;
            }
        }
        route6Entries++;
    }
}

void Route_print()
{
    printf("Stored routes:\n");
    for(uint64_t i = 0; i < routeEntries; i++)
    {
        char tmp[100];
        inet_ntop(AF_INET, &(routes[i].address), tmp, 100);
        printf("%s", tmp);
        inet_ntop(AF_INET, &(routes[i].netmask), tmp, 100);
        printf(" netmask %s", tmp);
        inet_ntop(AF_INET, &(routes[i].gateway), tmp, 100);
        printf(" via %s\n", tmp);
    }
    for(uint64_t i = 0; i < route6Entries; i++)
    {
        char tmp[200];
        inet_ntop(AF_INET6, &(routes6[i].address), tmp, 200);
        printf("%s", tmp);
        inet_ntop(AF_INET6, &(routes6[i].netmask), tmp, 200);
        printf(" netmask %s", tmp);
        inet_ntop(AF_INET6, &(routes6[i].gateway), tmp, 200);
        printf(" via %s\n", tmp);
    }
}

/**
 * @brief Get and verify route data
 * @param s Netlink socket descriptor
 * @param buf Buffer to store data into
 * @param maxBuf Buffer size
 * @param family Address family for routes (AF_INET or AF_INET6)
 * @return Received bytes count or -1 on failure
**/
int64_t route_getAndVerify(int s, uint8_t *buf, size_t maxBuf, sa_family_t family)
{
    struct nlmsghdr *nl = (struct nlmsghdr*)buf; //netlink header
    struct rtmsg *rt = (struct rtmsg*) NLMSG_DATA(nl); //route header
    int seq = 0; //sequence number

    nl->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)); //netlink message with route payload
    nl->nlmsg_type = RTM_GETROUTE; //get route command
    nl->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST; //return all entries (dump routing table). Request flag must be set on all requests (as linux manual says)
    nl->nlmsg_seq = seq;
    nl->nlmsg_pid = getpid(); //process PID must be passed
    memset(rt, 0, sizeof(struct rtmsg));
    rt->rtm_family = family; //seems that netlink returns only IPv4 routes when family=0 (but it should be a wildcard...)

    if(send(s, nl, nl->nlmsg_len, 0) < 0) //write request
    {
        DEBUG("Netlink write failed");
        return -1;
    }

    int64_t readSize = 0; //number of bytes returned by recv()
    size_t bufSize = 0; //current buf size when processing

    do 
    {
        readSize = recv(s, buf, maxBuf - bufSize, 0); //try to receive
        if(readSize < 0) //error
        {
            DEBUG("Netlink read failed");
            return -1;
        }

        if((NLMSG_OK(nl, readSize) == 0) || (nl->nlmsg_type == NLMSG_ERROR)) //check header validity
        {
            PRINT("Received netlink header is invalid!\n");
            return -1;
        }

        if(nl->nlmsg_type == NLMSG_DONE) //this was the last header
        {
            break;
        } 
        else //this wasn't the last header
        {
            buf += readSize; //move buffer pointer
            bufSize += readSize; //store received data count
        }

        if((nl->nlmsg_flags & NLM_F_MULTI) == 0)  //this is not a multipart message
        {
            break; //finish now
        }
    } 
    while ((nl->nlmsg_seq != seq) || (nl->nlmsg_pid != getpid()));

    return bufSize;
}

in_addr_t Route_get(in_addr_t address)
{
    for(uint64_t i = 0; i < routeEntries; i++)
    {
        if((address & routes[i].netmask.s_addr) == routes[i].address.s_addr) //route is matching
        {
            return routes[i].gateway.s_addr;    
        }
    }

    return 0; //no matching route
}


//compare function for IPv4 routing table sort
int sort_compare4(const void *a, const void *b)
{
    struct Route_s *s1 = (struct Route_s*)a;
    struct Route_s *s2 = (struct Route_s*)b;
    
    // if(ntohl(s1->address.s_addr) < ntohl(s2->address.s_addr))
    //     return -1;
    // else if(ntohl(s1->address.s_addr) > ntohl(s2->address.s_addr))
    //     return 1;
    // else
    // {
    //     return (ntohl(s1->netmask.s_addr) < ntohl(s2->netmask.s_addr)) - (ntohl(s1->netmask.s_addr) > ntohl(s2->netmask.s_addr));
    // }

    if(ntohl(s1->netmask.s_addr) < ntohl(s2->netmask.s_addr))
        return 1;
    else if(ntohl(s1->netmask.s_addr) > ntohl(s2->netmask.s_addr))
        return -1;
    else
    {
        return (ntohl(s1->address.s_addr) > ntohl(s2->address.s_addr)) - (ntohl(s1->address.s_addr) < ntohl(s2->address.s_addr));
    }

    return 0; //dummy
}

//compare function for IPv6 routing table sort
// int sort_compare6(struct Route6_s *s1, struct Route6_s *s2)
// {
//     if(s1->address.s_addr < s2->address.s_addr)
//         return -1;
//     else if(s1->address.s_addr > s2->address.s_addr)
//         return 1;
//     else
//     {
//         return (s1->netmask.s_addr < s2->netmask.s_addr) - (s1->netmask.s_addr > s2->netmask.s_addr);
//     }

//     return 0; //dummy
// }

int Route_update()
{
    free(routes);
    free(routes6);
    routeEntries = 0;
    route6Entries = 0;
    
    int s; //netlink socket handler
    uint8_t buf[NETLINK_BUF_SIZE]; //netlink data buffer
    memset(buf, 0, NETLINK_BUF_SIZE); //clear buffer

    if((s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) //create netlink socket
    {
        DEBUG("Netlink socket creation failed");
        return -1;
    }

    int64_t len = route_getAndVerify(s, buf, NETLINK_BUF_SIZE, AF_INET); //get IPv4 routes and verify
    if(len < 0)
    {
        close(s);
        return -1;
    }

    struct nlmsghdr *nl = (struct nlmsghdr*)buf; //get initial netlink message

    for (; NLMSG_OK(nl, len); nl = NLMSG_NEXT(nl, len)) 
    {
        route_parseAndAdd(nl); //parse and add routes
    }

    close(s); //close socket and do everything one more time for IPv6

    memset(buf, 0, NETLINK_BUF_SIZE);


    if((s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) //create netlink socket
    {
        DEBUG("Netlink socket creation failed");
        return -1;
    }

    len = route_getAndVerify(s, buf, NETLINK_BUF_SIZE, AF_INET6); //get IPv6 routes and verify
    if(len < 0)
    {
        close(s);
        return -1;
    }

    nl = (struct nlmsghdr*)buf; //get initial netlink message

    for (; NLMSG_OK(nl, len); nl = NLMSG_NEXT(nl, len)) 
    {
        route_parseAndAdd(nl); //parse and add routes
    }

    close(s); //close netlink socket


    qsort(routes, routeEntries, sizeof(*routes), sort_compare4); //sort route table by destination (in ascending order) first, then by netmask (in descendig order)
    //qsort(routes6, route6Entries, sizeof(*routes6), sort_compare6);

    return 0;
}

int Route_init()
{
    return 0;
}