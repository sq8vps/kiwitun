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

#include "tun.h"
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>

#define TUN_CLONE_DEV_PATH "/dev/net/tun" //tun clone device path

int Tun_create(char *name)
{
    struct ifreq ifr; // interface request structure
    int fd;           // file handle
    int ret;          // return value

    if((fd = open(TUN_CLONE_DEV_PATH, O_RDWR)) < 0) //open clone device
    {
        return fd; //error is returned in fd
    }

    memset(&ifr, 0, sizeof(ifr)); //zero out the structure

    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; //interface is TUN type, do not add packet information bytes

    if(name[0])
    {
        strncpy(ifr.ifr_name, name, IFNAMSIZ); //copy name if specified
    }

    if((ret = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) //create interface
    {
        close(fd); //failure - close and return
        return ret;
    }

    int dummy = socket(AF_INET, SOCK_DGRAM, 0); //create dummy socket for next ioctls (doesn't work with tun file descriptor)

    if((ret = ioctl(dummy, SIOCGIFFLAGS, (void*)&ifr)) < 0) //get flags
    {
        close(fd); //failure - close and return
        close(dummy);
        return ret;
    }

    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING); //turn on tunnel
    if((ret = ioctl(dummy, SIOCSIFFLAGS, (void*)&ifr)) < 0)
    {
        close(fd); //failure - close and return
        close(dummy);
        return ret;
    }

    close(dummy);

    //if no name has been provided, the kernel chose some
    //return this name to caller
    strcpy(name, ifr.ifr_name);

    return fd; //return file handle
}
