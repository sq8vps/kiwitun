
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

int tunfd = 0; //tun descriptor
int sockfd = 0; //IPIP socket descriptor

void sigterm_handler(int signum)
{
    if(tunfd > 0)
    {
        close(tunfd);
        DEBUG("Closing tunnel");
    }
    if(sockfd > 0)
    {
        close(tunfd);
        DEBUG("Closing IP socket");
    }
    PRINT("Exiting...\n");
    exit(0);
}

int main(int argc, char **argv)
{
    config.debug = 1;
    config.ttl = DEFAULT_IPV4_TTL;
    
    //set sigterm handler
    struct sigaction sa;
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

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
        close(sockfd);
        exit(0);
    }
    DEBUG("IPIP tunnel creation");

    setAddress(&(config.local), "0.0.0.0");
    setAddress(&(config.remote), "1.0.0.1");

    //start tunnel execution
    if(Ipip_exec() < 0)
    {
        DEBUG("IPIP tunneling failed");
        close(tunfd);
        close(sockfd);
        exit(0);
    }

    return 0;
}