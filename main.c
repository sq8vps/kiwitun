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

/**
 * @file main.c
 * @brief Main file - program entry.
 * 
 * Sets up everything, handles hostname resolution and daemonization.
*/

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
#include <sys/stat.h>
#include <syslog.h>

int tunfd = 0; //tun descriptor

//SIGINT handler
void sigintHandler(int signum)
{
    close(tunfd);
    closelog();
    PRINT(LOG_INFO, "Terminating...\n");
    exit(0);
}

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
                PRINT(LOG_DEBUG, "%s is at %s\n", config.hostname, tmp);
            }
        }
        else //no address was found
        {
            DEBUG(LOG_WARNING, "Hostname resolution failed");
        }
        
        
        if(config.hostnameRefresh) //there is a non-zero refresh time configured
            alarm(config.hostnameRefresh * 60); //set next alarm
    }
}

/**
 * @brief Daemonize program 
**/
void daemonize()
{
    pid_t pid = fork(); //try to fork

    if(pid < 0)
    {
        PRINT(LOG_ERR, "Daemonization failed\n");
        exit(-1);
    }

    if(pid > 0) //fork successful, PID returned in parent
    {
        exit(0); //terminate parent
    }

    if(setsid() < 0) //create new session
    {
        PRINT(LOG_ERR, "Daemonization failed\n");
        exit(-1);
    }

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGCHLD, &sa, NULL) < 0) //ignore SIGCHLD
    {
        DEBUG(LOG_ERR, "Daemonization failed");
        exit(-1);
    }

    pid = fork(); //fork again

    if(pid < 0)
    {
        PRINT(LOG_ERR, "Daemonization failed\n");
        exit(-1);
    }

    if(pid > 0) //fork successful, PID returned in parent
    {
        exit(0); //terminate parent
    }

    umask(0);
    chdir("/");

	for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) //close all open file descriptors
    {
		close(fd);
	}

    //reopen standard streams and redirect them to /dev/null
    stdin = fopen("/dev/null", "r");
	stdout = fopen("/dev/null", "w+");
	stderr = fopen("/dev/null", "w+");
}

int main(int argc, char **argv)
{   
    //initial settings
    config.ttl = DEFAULT_IPV4_TTL;
    config.hostnameRefresh = DEFAULT_HOSTNAME_REFRESH;
    config.debug = 0;
    config.hostname = NULL;
    config.ifName = NULL;
    config.noDaemon = 0;
    config.tun4in4 = 0;
    config.tun6in4 = 0;
    config.logLevel = 255;
    setAddress(&(config.local), "0.0.0.0");
    setAddress(&(config.remote), "0.0.0.0");
    setAddress6(&(config.local6), "::");
    setAddress6(&(config.remote6), "::");

    if(parseArgs(argc, argv) < 0) //parse arguments
        exit(-1); //exit if something was wrong

    if(getuid() != 0)
    {
        printf("kiwitun must be run as a root\n");
        exit(-1);
    }

    if(config.logLevel == 255) //log level not set explicitly
    {
        if(config.debug) //but there is a "verbose/debug" option set
            config.logLevel = LOG_DEBUG; //log/print everything
        else //no verbose option
            config.logLevel = LOG_INFO; //log/print everything but debug information
    }

    config.logLevel = (config.logLevel > LOG_DEBUG) ? LOG_DEBUG : config.logLevel;    

    if(!config.noDaemon) //daemonization enabled
    {
        setlogmask(LOG_UPTO(config.logLevel)); //set max level to log
        openlog("kiwitun", LOG_CONS | LOG_PID, LOG_DAEMON); //open log
        daemonize(); //try do daemonize
    }

    PRINT(LOG_DEBUG, "Starting kiwitun with following settings:\n");
    PRINT(LOG_DEBUG, "4-in-4 tunneling: %d\n6-in-4 tunneling: %d\n", (int)config.tun4in4, (int)config.tun6in4);
    PRINT(LOG_DEBUG, "Local address: ");
    printAddress(LOG_DEBUG, &(config.local));
    PRINT(LOG_DEBUG, "\nRemote address: ");
    if(config.hostname != NULL)
    {
        PRINT(LOG_DEBUG, "%s", config.hostname);
    }
    else
        printAddress(LOG_DEBUG, &(config.remote));
    
    PRINT(LOG_DEBUG, "\nDebugging output: %d\nStart as a daemon: %d\n", (int)config.debug, (int)(!config.noDaemon));
    PRINT(LOG_DEBUG, "Interface name: ");
    if(config.ifName != NULL)
    {
        PRINT(LOG_DEBUG, "%s\n", config.ifName);
    }
    else
    {
        PRINT(LOG_DEBUG, "not specified\n");
    }
    PRINT(LOG_DEBUG, "TTL/hop limit: %d\nHostname resolution interval: %u minutes\n", (int)config.ttl, (unsigned int)config.hostnameRefresh);

    struct sigaction sa;
    sa.sa_handler = &alarmHandler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGALRM, &sa, NULL) < 0) //attach alarm handler
    {
        DEBUG(LOG_ERR, "Alarm handler attachment failure");
        exit(-1);
    }

    sa.sa_handler = &sigintHandler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGINT, &sa, NULL) < 0) //attach SIGINT handler
    {
        DEBUG(LOG_ERR, "SIGINT handler attachment failure");
        exit(-1);
    }

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
       DEBUG(LOG_ERR, "TUN interface creation failed");
       exit(-1);
    }

    PRINT(LOG_INFO, "\n\nTunnel interface name is %s\n", ifName);

    //initialize tunneling
    if(Ipip_init(tunfd) < 0)
    {
        DEBUG(LOG_ERR, "IPIP tunnel creation failed");
        exit(-1);
    }

    //start tunneling engine
    if(Ipip_start() < 0)
    {
        DEBUG(LOG_ERR, "IPIP tunneling failed");
        exit(-1);
    }

    PRINT(LOG_INFO, "Started succesfully\n");

    while(1)
    {
        pause(); //pause thread and wait for signals
    }

    return 0;
}

