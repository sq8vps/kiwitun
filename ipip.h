#ifndef IPIP_H_
#define IPIP_H_

#define IPV4_MAX_PACKET_SIZE 65535
#define IPV4_HEADER_SIZE 20
#define IPV4_HEADER_CHECKSUM_POS 10

#define IPV4_HEADER_PROTO_IPIP 0x04
#define IPV4_HEADER_PROTO_IP6IP 0x29

#define IPVX_HEADER_VERSION_4 0x04
#define IPVX_HEADER_VERSION_6 0x06

#define IPV6_HEADER_SIZE 40

#include <stdint.h>

int Ipip_init(int tun);
int Ipip_exec();


#endif