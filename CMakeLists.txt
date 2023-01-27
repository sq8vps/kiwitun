cmake_minimum_required(VERSION 3.0.0)
project(kiwitun VERSION 0.1.0)

add_executable(kiwitun main.c
                tun.c tun.h
                ipip.c ipip.h
                common.c common.h
                icmp.c icmp.h
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
