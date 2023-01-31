/**
 * @file route.c
 * @brief Route selection module
 * 
 * This module maintains local routing tables acquired from kernel and provides a route lookup for given destination address.
 * It is shell- and system files-independent module that uses only netlink and rtnetlink Linux kernel functionalities.
 * Netlink and especially rtnetlink are VERY poorly documented. The code for reading routes from kernel is based
 * mainly on this answer: https://stackoverflow.com/a/3288983 (https://stackoverflow.com/questions/3288065/getting-gateway-to-use-for-a-given-ip-in-ansi-c)
 * The code for listening for route updates is based mainly on this solution: https://olegkutkov.me/2018/02/14/monitoring-linux-networking-state-using-netlink/
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
#include <signal.h>
#include <pthread.h>

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

struct RouteHelper_s
{
    union
    {
        struct Route_s route4;
        struct Route6_s route6;
    } r;
};


struct Route_s *routes = NULL; //local IPv4 routing table
uint16_t routeBlocks = 0; //number of reserved blocks for routing table
uint64_t routeEntries = 0; //number of entries in routing table
pthread_mutex_t	routesMutex = PTHREAD_MUTEX_INITIALIZER; //IPv4 routing table access mutex
#define LOCK_ROUTES() (pthread_mutex_lock(&routesMutex))
#define UNLOCK_ROUTES() (pthread_mutex_unlock(&routesMutex))

struct Route6_s *routes6 = NULL; //local IPv6 routing table
uint16_t route6Blocks = 0; //number of reserved blocks for routing table
uint64_t route6Entries = 0; //number of entries in routing table
pthread_mutex_t	routes6Mutex = PTHREAD_MUTEX_INITIALIZER; //IPv6 routing table access mutex
#define LOCK_ROUTES6() (pthread_mutex_lock(&routes6Mutex))
#define UNLOCK_ROUTES6() (pthread_mutex_unlock(&routes6Mutex))




//compare function for IPv4 routing table sort
int sort_compare4(const void *a, const void *b)
{
    struct Route_s *s1 = (struct Route_s*)a;
    struct Route_s *s2 = (struct Route_s*)b;

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
int sort_compare6(const void *a, const void *b)
{
    struct Route6_s *s1 = (struct Route6_s*)a;
    struct Route6_s *s2 = (struct Route6_s*)b;
    
    if(ipv6_compare(s1->netmask, s2->netmask) == -1)
        return 1;
    else if(ipv6_compare(s1->netmask, s2->netmask) == 1)
        return -1;
    else
    {
        return ipv6_compare(s1->address, s2->address);
    }
    return 0; //dummy
}

//sort IPv4 routing table
void route_sort()
{
    LOCK_ROUTES();
    qsort(routes, routeEntries, sizeof(*routes), sort_compare4);
    UNLOCK_ROUTES();
}

//sort IPv6 routing table
void route_sort6()
{
    LOCK_ROUTES6();
    qsort(routes6, route6Entries, sizeof(*routes6), sort_compare6);
    UNLOCK_ROUTES6();
}

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
 * @brief Parse route data
 * @param nl Netlink message 
 * @param route Decoded route
 * @param family Decoded route family: AF_INET, AF_INET6 or AF_UNSPEC is returned
 * @attention If returned family is AF_UNSPEC the route must be discarded
**/
void route_parse(struct nlmsghdr *nl, struct RouteHelper_s *route, int *family)
{
    *family = AF_UNSPEC; //initialize family as unspecified
    
    struct rtmsg *rt = (struct rtmsg *)NLMSG_DATA(nl); //get route message

    struct rtattr *rtAttr = (struct rtattr *)RTM_RTA(rt); //get first route attribute
    int len = RTM_PAYLOAD(nl);

    if(rt->rtm_family == AF_INET) //an IPv4 route
    {
        if(rt->rtm_type != RTN_UNICAST) //accept only gateways and direct routes
            return;

        route->r.route4.address.s_addr = INADDR_ANY;
        route->r.route4.gateway.s_addr = INADDR_ANY;
        route->r.route4.netmask.s_addr = CIDR_TO_ADDR4(rt->rtm_dst_len); //get netmask length and convert it to address

        for (; RTA_OK(rtAttr, len); rtAttr = RTA_NEXT(rtAttr, len)) //go through all attributes
        {
            switch(rtAttr->rta_type) 
            {
                case RTA_GATEWAY: //get gateway
                    route->r.route4.gateway.s_addr = *(in_addr_t*)RTA_DATA(rtAttr);
                    break;
                case RTA_DST: //get destination
                    route->r.route4.address.s_addr = *(in_addr_t*)RTA_DATA(rtAttr);
                    break;
                default:
                    break;
            }
        }

        if(route->r.route4.address.s_addr != INADDR_ANY) //is this not a default gateway?
        {
            *family = AF_INET;
        }
        //omit default gateways
    }
    else if(rt->rtm_family == AF_INET6) //an IPv6 route
    {
        route->r.route6.netmask = in6addr_any;
        route->r.route6.address = in6addr_any;
        route->r.route6.gateway = in6addr_any;
        route->r.route6.netmask = CIDR_TO_ADDR6(rt->rtm_dst_len); //get netmask length and convert it to address

        for (; RTA_OK(rtAttr, len); rtAttr = RTA_NEXT(rtAttr, len)) //go through all attributes
        {
            if(rt->rtm_type != RTN_UNICAST) //accept only gateways or direct routes
                return;
            
            switch(rtAttr->rta_type) 
            {
                case RTA_GATEWAY: //get gateway
                    route->r.route6.gateway = *(struct in6_addr*)RTA_DATA(rtAttr);
                    break;
                case RTA_DST: //get destination
                    route->r.route6.address = *(struct in6_addr*)RTA_DATA(rtAttr);
                    break;
                default:
                    break;
            }
        }

        if(ipv6_isEqual(route->r.route6.address, in6addr_any) == 0) //is this not a default gateway?
        {
            *family = AF_INET6;
        }
        //omit default gateways
    }
}

/**
 * @brief Insert IPv4 route to the table (and resize if necessary) - thread safe
 * @param r Route to insert
 * @return 0 on success, -1 on failure
**/
int route_insert(struct Route_s *r)
{
    LOCK_ROUTES();
    if(route_resizeTable(routeEntries + 1) < 0) //store route
    {
        UNLOCK_ROUTES();
        return -1;
    }
    routes[routeEntries] = *r;
    routeEntries++;
    UNLOCK_ROUTES();
    return 0;
}

/**
 * @brief Insert IPv6 route to the table (and resize if necessary) - thread safe
 * @param r Route to insert
 * @return 0 on success, -1 on failure
**/
int route_insert6(struct Route6_s *r)
{
    LOCK_ROUTES6();
    if(route_resizeTable6(route6Entries + 1) < 0) //store route
    {
        UNLOCK_ROUTES6();
        return -1;
    }
    routes6[route6Entries] = *r;
    route6Entries++;
    UNLOCK_ROUTES6();
    return 0;
}

/**
 * @brief Find matching IPv4 route in table, remove it and shift table
 * @param r Route to remove 
**/
void route_removeAndShift(struct Route_s *r)
{
    LOCK_ROUTES();
    for(uint64_t i = 0; i < routeEntries; i++)
    {
        if((routes[i].address.s_addr == r->address.s_addr) && (routes[i].netmask.s_addr == r->netmask.s_addr) && (routes[i].gateway.s_addr == r->gateway.s_addr))
        {
            //matching route found
            for(uint64_t j = i; j < (routeEntries - 1); j++)
            {
               routes[j] = routes[j + 1]; //shift all routes replacing the one being removed
            }
            routeEntries--;
            break;
        }
    }
    UNLOCK_ROUTES();
}

/**
 * @brief Find matching IPv6 route in table, remove it and shift table
 * @param r Route to remove 
**/
void route_removeAndShift6(struct Route6_s *r)
{
    LOCK_ROUTES6();
    for(uint64_t i = 0; i < route6Entries; i++)
    {
        if(ipv6_compare(routes6[i].address, r->address) && ipv6_compare(routes6[i].netmask, r->netmask) && ipv6_compare(routes6[i].gateway, r->gateway))
        {
            //matching route found
            for(uint64_t j = i; j < (route6Entries - 1); j++)
            {
               routes6[j] = routes6[j + 1]; //shift all routes replacing the one being removed
            }
            route6Entries--;
            break;
        }
    }
    UNLOCK_ROUTES6();
}



void Route_print()
{
    LOCK_ROUTES();
    LOCK_ROUTES6();
    char tmp[200];
    printf("Stored routes:\n");
    for(uint64_t i = 0; i < routeEntries; i++)
    {
        inet_ntop(AF_INET, &(routes[i].address), tmp, 200);
        printf("%s", tmp);
        inet_ntop(AF_INET, &(routes[i].netmask), tmp, 200);
        printf(" netmask %s", tmp);
        inet_ntop(AF_INET, &(routes[i].gateway), tmp, 200);
        printf(" via %s\n", tmp);
    }
    for(uint64_t i = 0; i < route6Entries; i++)
    {
        inet_ntop(AF_INET6, &(routes6[i].address), tmp, 200);
        printf("%s", tmp);
        inet_ntop(AF_INET6, &(routes6[i].netmask), tmp, 200);
        printf(" netmask %s", tmp);
        inet_ntop(AF_INET6, &(routes6[i].gateway), tmp, 200);
        printf(" via %s\n", tmp);
    }
    UNLOCK_ROUTES();
    UNLOCK_ROUTES6();
}

/**
 * @brief Get and verify route data
 * @param s Netlink socket descriptor
 * @param buf Buffer to store data into
 * @param maxBuf Buffer size
 * @param family Address family for routes (AF_INET or AF_INET6)
 * @return Received bytes count or -1 on failure
**/
int64_t route_receive(int s, uint8_t *buf, size_t maxBuf, int seq)
{
    int64_t readSize = 0; //number of bytes returned by recv()
    size_t bufSize = 0; //current buf size while processing

    struct nlmsghdr *nl;

    do 
    {
        readSize = recv(s, buf, maxBuf - bufSize, 0); //try to receive
        if(readSize < 0) //error
        {
            DEBUG("Netlink read failed");
            return -1;
        }

        nl = (struct nlmsghdr*)buf;

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
    LOCK_ROUTES();
    for(uint64_t i = 0; i < routeEntries; i++)
    {
        if((address & routes[i].netmask.s_addr) == routes[i].address.s_addr) //route is matching
        {
            in_addr_t ret = routes[i].gateway.s_addr;
            UNLOCK_ROUTES();
            return ret;    
        }
    }
    UNLOCK_ROUTES();
    return 0; //no matching route
}


int route_NLrequestAll(int s, uint8_t *buf, size_t maxBuf, sa_family_t family)
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
        close(s);
        return -1;
    }

    int64_t len = route_receive(s, buf, maxBuf, seq);
}

void *route_listenForUpdates(void *arg)
{
    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;
    addr.nl_pid = getpid();

    int s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if(s < 0)
    {
        DEBUG("Netlink socket open failed");
        return (void*)-1;
    }

    if(bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        DEBUG("Netlink socket bind failed");
        return (void*)-1;
    }

    uint8_t buf[NETLINK_BUF_SIZE]; //netlink data buffer
    memset(buf, 0, NETLINK_BUF_SIZE); //clear buffer

    int size = 0; //received data size
    struct nlmsghdr *nl; //received netlink header
    struct RouteHelper_s route; //received route buffer
    int family = AF_UNSPEC; //received route family

    while(1)
    {
        size = recv(s, buf, NETLINK_BUF_SIZE, 0); //try to receive
        if(size < 0) //error
        {
            DEBUG("Netlink read failed");
            continue;
        }

        nl = (struct nlmsghdr*)buf;

        if((NLMSG_OK(nl, size) == 0) || (nl->nlmsg_type == NLMSG_ERROR)) //check header validity
        {
            PRINT("Received netlink header is invalid!\n");
            continue;
        }

        route_parse(nl, &route, &family); //parse route
        if(family == AF_INET) //IPv4 route
        {
            if(nl->nlmsg_type == RTM_NEWROUTE) //this is a new route
            {
                route_insert(&route.r.route4); //insert route
                route_sort(); //sort table
            }
            else if(nl->nlmsg_type == RTM_DELROUTE) //this route needs to be deleted
            {
                route_removeAndShift(&route.r.route4); //remove route
            }
        }
        else if(family == AF_INET6) //IPv6 route
        {
            if(nl->nlmsg_type == RTM_NEWROUTE) //this is a new route
            {
                route_insert6(&route.r.route6); //insert route
                route_sort6(); //sort table
            }
            else if(nl->nlmsg_type == RTM_DELROUTE) //this route needs to be deleted
            {
                route_removeAndShift6(&route.r.route6); //remove route
            }
        }
        
    }
    return (void*)-1;
}

/**
 * @brief Get and store all available routes
 * @return 0 on success, -1 on failure
**/
int route_getAll()
{
    free(routes);
    free(routes6);
    routeEntries = 0;
    route6Entries = 0;
    
    int s; //netlink socket handler
    uint8_t buf[NETLINK_BUF_SIZE]; //netlink data buffer
    memset(buf, 0, NETLINK_BUF_SIZE); //clear buffer

    struct nlmsghdr *nl = (struct nlmsghdr*)buf; //get initial netlink message

    //received route buffer
    struct RouteHelper_s route;
    //received route family
    int family = AF_UNSPEC;

    if((s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) //create netlink socket
    {
        DEBUG("Netlink socket creation failed");
        return -1;
    }

    int64_t len = route_NLrequestAll(s, buf, NETLINK_BUF_SIZE, AF_INET); //get IPv4 routes and verify
    if(len < 0)
    {
        close(s);
        return -1;
    }

    for (; NLMSG_OK(nl, len); nl = NLMSG_NEXT(nl, len)) //parse all messages
    {
        route_parse(nl, &route, &family); //parse and add routes
        if(family == AF_INET) //check if family matches
        {
            route_insert(&route.r.route4); //insert route
        }
    }

    close(s); //close socket and do everything one more time for IPv6

    memset(buf, 0, NETLINK_BUF_SIZE);

    if((s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) //create netlink socket
    {
        DEBUG("Netlink socket creation failed");
        return -1;
    }

    len = route_NLrequestAll(s, buf, NETLINK_BUF_SIZE, AF_INET6); //get IPv6 routes and verify
    if(len < 0)
    {
        close(s);
        return -1;
    }

    nl = (struct nlmsghdr*)buf; //get initial netlink message

    for (; NLMSG_OK(nl, len); nl = NLMSG_NEXT(nl, len)) 
    {
        route_parse(nl, &route, &family); //parse and add routes
        if(family == AF_INET6) //check if family matches
        {
            route_insert6(&route.r.route6); //insert route
        }
    }

    close(s); //close netlink socket

    route_sort(); //sort route table by netmask (in descending order) first, then by destionation (in ascendig order)
    route_sort6();

    return 0;
}

int Route_init()
{
    if(route_getAll() < 0) //get all routes
        return -1;

    pthread_t listener;
    
    if(pthread_create(&listener, NULL, &route_listenForUpdates, NULL) < 0) //create listener thread
    {
        DEBUG("Listener thread creation failed");
        return -1;
    }

    return 0;
}