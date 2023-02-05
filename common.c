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

#include "common.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <linux/if.h>

struct Config_s config;

int setAddress(struct in_addr *s, char *addr)
{
    if(inet_pton(AF_INET, addr, s) < 1) //convert
    {
        return -1;
    }
    return 0;
}

int setAddress6(struct in6_addr *s, char *addr)
{
    if(inet_pton(AF_INET6, addr, s) < 1) //convert
    {
        return -1;
    }
    return 0;
}

void printAddress(int logLevel, struct in_addr *addr)
{
    char tmp[200];
    inet_ntop(AF_INET, addr, tmp, 200);
    PRINT(logLevel, "%s", tmp);
}

void printAddress6(int logLevel, struct in6_addr *addr)
{
    char tmp[200];
    inet_ntop(AF_INET6, addr, tmp, 200);
    PRINT(logLevel, "%s", tmp);
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
        ret.__in6_u.__u6_addr32[i] = a1.__in6_u.__u6_addr32[i] & a2.__in6_u.__u6_addr32[i];

    return ret;
}

const char helpPage[];

int parseArgs(int argc, char **argv)
{
    #define ARG_REFRESH 128
    #define ARG_VERSION 129
    #define ARG_LOGLEVEL 130
    struct option options[] =
    {
        {"verbose", no_argument, 0, 'v'},
        {"4in4", no_argument, 0, '4'},
        {"6in4", no_argument, 0, '6'},
        {"remote", required_argument, 0, 'r'},
        {"local", required_argument, 0, 'l'},
        {"refresh", required_argument, 0, ARG_REFRESH},
        {"ttl", required_argument, 0, 't'},
        {"version", no_argument, 0, ARG_VERSION},
        {"interface", no_argument, 0, 'i'},
        {"no-daemon", no_argument, 0, 'd'},
        {"log-level", required_argument, 0, ARG_LOGLEVEL},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int c;

    while (1)
    {
        int index = 0;

        c = getopt_long(argc, argv, "v46r:l:t:i:dh", options, &index);

        if (c == -1)
            break;

        switch (c)
        {
            case 0:
            break;

            case 'v': //verbose output
            config.debug = 1;
            break;

            case '4': //enable 4in4 tunneling
            config.tun4in4 = 1;
            break;

            case '6': //enable 6in4 tunneling
            config.tun6in4 = 1;
            break;

            case 'r': //set remote IPv4
            if(setAddress(&(config.remote), optarg) < 0) //parse and set if IP
            {
                //if not IP, store as hostname
                setAddress(&(config.remote), "0.0.0.0"); //zero-out first
                config.hostname = malloc(strlen(optarg) + 1);
                if(config.hostname == NULL)
                {
                    printf("malloc failure\n");
                    return -1;
                }
                strcpy(config.hostname, optarg);
            }
            break;

            case 'l': //set local IPv4
            if(setAddress(&(config.local), optarg) < 0) //parse and set if IP
            {
                printf("Local tunnel endpoint IPv4 address %s is invalid.\n", optarg);
                return -1;
            }
            break;

            case 't': //TTL/hop limit
            config.ttl = atoi(optarg);
            if(config.ttl == 0)
            {
                printf("TTL/hop limit must be in range 1 to 255.\n");
                return -1;
            }
            break;

            case ARG_REFRESH: //hostname refresh interval
            config.hostnameRefresh = atoi(optarg);
            break;

            case 'i': //interface name
            config.ifName = malloc(strlen(optarg) + 1);
            if(config.ifName == NULL)
            {
                printf("malloc failure\n");
                return -1;
            }
            strncpy(config.ifName, optarg, IFNAMSIZ);
            break;

            case 'd': //do not daemonise
            config.noDaemon = 1;
            break;

            case ARG_LOGLEVEL:
            config.logLevel = atoi(optarg);
            break;

            case ARG_VERSION: //version string
            printf(KIWITUN_VERSION_STRING);
            exit(0);
            break;

            case 'h': //print help page
            printf("%s", helpPage);
            exit(0);
            break;

            case ':':
            return -1;
            break;

            case '?':
            return -1;
            break;

            default:
            return -1;
            break;
        }
    }

    if(!config.tun4in4 && !config.tun6in4) //check if at least one mode is selected
    {
        printf(KIWITUN_VERSION_STRING);
        printf("\nTo start kiwitun at least one tunneling mode must be selected.\nUse \"kiwitun --help\" to print help page.\n");
        return -1;
    }


    return 0;
}

const char helpPage[] = "Usage: kiwitun [options]\n"\
                        "Tunneling modes:\n"\
                        " -4, --4in4\t\tenable IPIP (4in4) tunneling"\
                        " -6, --6in4\tenable IP6IP (6in4) tunneling"\
                        "Tunnel settings:\n"\
                        " -r, --remote=address\tuse given hostname or IP as a remote endpoint address. The routing table is used when remote hostname/address is not set\n"\
                        " -l, --local=address\tuse given IP as a local endpoint address. Kernel selects appropriate address if not set\n"\
                        " -t, --ttl=value\tuse given TTL/hop limit value for encapsulated and ICMP packets\n"\
                        " -i, --ifname=name\tuse given TUN interface name. Kernel selects appropriate name if not set\n"\
                        "Other settings:\n"\
                        " --refresh=time\tresolve remote endpoint hostname every given period of time (given in minutes)\n"\
                        " -d, --no-daemon\tdo not run as a daemon\n"\
                        " --log-level=level\tset logging level. Lower value means less logging. Valid values are 0 to 7 (values higher than 7 are clipped to 7). --log-level=7 is equivalent to --verbose. Setting it to 0 should disable logging\n"\
                        " -v, --verbose\t\tverbose/debug mode: print/log everything. Equivalent to --log-level=7\n"\
                        "Version and help:\n"\
                        " --version\t\tprint version information\n"\
                        " -h, --help\t\tprint help page\n";
