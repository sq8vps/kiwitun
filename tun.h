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
 * @file tun.h
 * @brief TUN interface module
 * 
 * Creates and sets up Linux TUN interface.
*/
#ifndef TUN_H_
#define TUN_H_

/**
 * @brief Create TUN interface
 * @param name Interface name or empty string for automatic selection. Must be an IFSIZNAME-long array.
 * @return Interface (file) descriptor or -1 on failure
*/
int Tun_create(char *name);

#endif