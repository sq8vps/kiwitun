#[[
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
]]

cmake_minimum_required(VERSION 3.0.0)
project(kiwitun VERSION 1.0.0)

add_executable(kiwitun main.c
                tun.c tun.h
                ipip.c ipip.h
                common.c common.h
                icmp.c icmp.h
                route.c route.h
)

target_link_libraries(kiwitun PUBLIC pthread)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
