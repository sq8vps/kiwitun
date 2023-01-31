
#include <stdio.h>
#include "tun.h"
#include "ipip.h"
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <linux/if.h>
#include <arpa/inet.h>
#include "common.h"
#include "route.h"

int tunfd = 0; //tun descriptor

int main(int argc, char **argv)
{
    config.debug = 1;
    config.tun4in4 = 1;
    config.tun6in4 = 0;
    config.ttl = DEFAULT_IPV4_TTL;
    Route_init();

    //create tun interface
    char name[IFNAMSIZ] = "\0";
    tunfd = Tun_create(name);
    if(tunfd < 0) //creation failure
    {
       DEBUG("TUN interface creation failed");
       exit(0); 
    }
    DEBUG("TUN interface creation");

    //create tunnel
    if(Ipip_init(tunfd) < 0)
    {
        DEBUG("IPIP tunnel creation failed");
        close(tunfd);
        exit(0);
    }
    DEBUG("IPIP tunnel creation");

    setAddress(&(config.local), "0.0.0.0");
    setAddress(&(config.remote), "0.0.0.0");

    //start tunnel execution
    if(Ipip_start() < 0)
    {
        DEBUG("IPIP tunneling failed");
        close(tunfd);
        exit(0);
    }



    while(1);

    return 0;
}