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
 * @file ipip.h
 * @brief IPIP tunneling module (4-in-4 and 6-in-4)
 * 
 * Creates all required sockets and handles IPIP encapsulation in decapsulation.
 * Currently supports only 4-in-4 and 6-in-4 tunneling.
*/
#ifndef IPIP_H_
#define IPIP_H_

#include <stdint.h>

/**
 * @brief Initialize tunneling module
 * @param tun TUN interface descriptor
 * @return 0 on success, -1 on failure
**/
int Ipip_init(int tun);

/**
 * @brief Start tunneling engine execution (non-blocking)
 * @return 0 on success, -1 on failure 
**/
int Ipip_start();


#endif