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
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int tunfd = 0; //tun descriptor

//alarm handler for hostname resolving
void alarmHandler(int signum)
{
    if(signum == SIGALRM)
    {
        struct addrinfo hints, *results;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        if(getaddrinfo(config.hostname, NULL, &hints, &results) >= 0) //get address for hostname
        {
            config.remote.s_addr = ((struct sockaddr_in*)(results->ai_addr))->sin_addr.s_addr; //store first address
            if(config.debug) //print if verbose output
            {
                char tmp[50];
                inet_ntop(AF_INET, &(config.remote), tmp, 50);
                PRINT("%s is at %s\n", config.hostname, tmp);
            }
        }
        else //no address was found
        {
            DEBUG("Hostname resolve failed");
        }
        
        
        if(config.hostnameRefresh) //there is a non-zero refresh time configured
            alarm(config.hostnameRefresh * 60); //set next alarm
    }
}

int main(int argc, char **argv)
{   
    //initial settings
    config.ttl = DEFAULT_IPV4_TTL;
    config.hostnameRefresh = DEFAULT_HOSTNAME_REFRESH;
    config.debug = 0;
    config.hostname = NULL;
    config.ifName = NULL;
    config.noDaemonise = 0;
    config.tun4in4 = 0;
    config.tun6in4 = 0;
    setAddress(&(config.local), "0.0.0.0");
    setAddress(&(config.remote), "0.0.0.0");
    setAddress6(&(config.local6), "::");
    setAddress6(&(config.remote6), "::");

    struct sigaction sa;
    sa.sa_handler = &alarmHandler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGALRM, &sa, NULL) < 0) //attach alarm handler
    {
        DEBUG("Alarm handler attachment failure");
        exit(-1);
    }

    if(parseArgs(argc, argv) < 0) //parse arguments
        exit(-1); //exit if something was wrong

    if(config.hostname != NULL) //there is a remote hostname configured
    {
        alarmHandler(SIGALRM); //call alarm handler to resolve hostname
    }

    if(Route_init() < 0) //initialize routing module
    {
        exit(-1);
    }

    //create tun interface
    char ifName[IFNAMSIZ] = "\0"; //NULL for automatic interface name selection
    if(config.ifName != NULL) //there is a name specified
        strcpy(ifName, config.ifName);

    tunfd = Tun_create(ifName); //use provided interface name if available
    if(tunfd < 0) //creation failure
    {
       DEBUG("TUN interface creation failed");
       exit(-1);
    }

    //initialize tunneling
    if(Ipip_init(tunfd) < 0)
    {
        DEBUG("IPIP tunnel creation failed");
        exit(-1);
    }

    //start tunneling engine
    if(Ipip_start() < 0)
    {
        DEBUG("IPIP tunneling failed");
        exit(-1);
    }

    PRINT("Started succesfully\n");

    // if(!config.noDaemonise) //daemonize
    //     daemon(0, 0);

    while(1)
    {
        pause(); //pause thread and wait for signals
    }

    return 0;
}