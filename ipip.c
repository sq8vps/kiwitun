#include "ipip.h"

#include <linux/if.h>
#include <sys/socket.h>

#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include "common.h"
#include "icmp.h"
#include "route.h"


#include <string.h>


static int sockfd = 0; //IPIP socket descriptor
static int sock6fd = 0; //IP6IP socket descriptor 
static int tunfd = 0; //tun descriptor

int Ipip_init(int tun)
{
    tunfd = tun;
    
    if(config.tun4in4) //enable 4-in-4 tunneling
    {
        if((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_IPIP)) < 0) //try to create raw IPv4 socket
        {
            return -1; //return if failure
        }

        int enable = 1;
        if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable)) < 0) //tell kernel to not add IP header
        {
            close(sockfd);
            return -1;
        }
    }

    if(config.tun6in4) //enable 6-in-4 tunneling
    {
        if((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_IPV6)) < 0) //try to create raw IPv4 socket
        {
            return -1; //return if failure
        }

        int enable = 1;
        if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable)) < 0) //tell kernel to not add IP header
        {
            close(sockfd);
            return -1;
        }
    }

    

    return 0;
}

/**
 * @brief Calculate IPv4 header checksum
 * @param hdr Header data pointer
 * @param size Header size in bytes
 * @return 0 if success, -1 otherwise (size not a multiplicity of 4)
 * @warning New checksum is inserted into the header.
**/
int ipv4_checksum(uint8_t *hdr, uint16_t size)
{
    if(size & 3) //size must be divisible by 4 
        return -1;

    uint32_t sum = 0;
    for(uint16_t i = 0; i < size; i += 2) //group data in 16-bit words
    {
        if(i == IPV4_HEADER_CHECKSUM_POS) //skip checksum field itself (treat is a 0x0000)
            continue;

        sum += (((uint32_t)hdr[i] << 8) | (uint32_t)hdr[i + 1]); //convert to 16-bit words and add to the sum
    }
    sum = (sum & 0xFFFF) + (sum >> 16); //cut carry bits, shift them and add to the checksum
    if(sum & 0x10000) //check if addition generated carry bit
    {
        sum &= 0xFFFF; //if so, remove it
        sum++; //and add one to the checksum
    }
    sum = ~sum; //bitwise negate (one's complement)
    hdr[IPV4_HEADER_CHECKSUM_POS] = (sum >> 8) & 0xFF; //store checksum
    hdr[IPV4_HEADER_CHECKSUM_POS + 1] = sum & 0xFF;

    return 0;
}

/**
 * @brief Get tunnel destination (remote) address for packet being encapsulated
 * @param addr Inner packet destination address
 * @return Tunnel remote address 
**/
in_addr_t ipip_getDestination(in_addr_t addr)
{
    if(config.remote.s_addr != 0) //there is a fixed remote IP address defined
        return config.remote.s_addr; //use it

    return Route_get(addr); //else get from routing table
}

/**
 * @brief Encapsulate IPv4 packet and send as IPv4 packet
 * @param buf Packet buffer with additional space for outer header (inner header must at buf + IPV4_HEADER_SIZE)
 * @param size Size of packet being encapsulated (inner data excluding space left for outer header)
 * @return 0 if success, -1 otherwise
**/
int ipip_encap(uint8_t *buf, int size)
{
    struct ip *outer = (struct ip*)buf; //set pointer to outer IP header
    struct ip *inner = (struct ip*)(&buf[IPV4_HEADER_SIZE]); //get inner IP header

    if(inner->ip_v != IPVX_HEADER_VERSION_4) //not IPv4 packet somehow
        return -1;

    if(inner->ip_hl != (IPV4_HEADER_SIZE / 4)) //drop packets than don't have standard headers
    {
        PRINT("Blocking IPv4 packet with header length other than %d bytes.\n", IPV4_HEADER_SIZE);
        return -1;
    }

    //when receiving packet its length is restricted to (max IP packet size - IPv4 header size), so that it can be encapsulated
    //check if packet is too big for encapsulation (or length field in header is broken)
    if(ntohs(inner->ip_len) != size)
    {
        PRINT("Packet received on tunnel interface has inconsistent size or is too big to be tunneled\n");
        return -1;
    }

    //TTL behavior according to RFC 2003
    if(inner->ip_ttl == 0) //TTL=0, drop packet
        return 0;
    else if(inner->ip_ttl == 1) //TTL=1, will be 0 after decrementation, send ICMP Time Exceeded
    {
        //time exceeded, send ICMP response: Time Exceeded
        return ICMP_send(sockfd, &(buf[IPV4_HEADER_SIZE]), size, (config.local.s_addr == 0) ? 0 : config.local.s_addr,
                    ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, 0);
    }

    inner->ip_ttl--; //decrement TTL
    ipv4_checksum((uint8_t*)inner, inner->ip_hl * 4); //recalculate checksum
    
    //fill outer header
    outer->ip_v = IPVX_HEADER_VERSION_4; //IPv4 packet
    outer->ip_hl = IPV4_HEADER_SIZE / 4; //header size in 32-bit units
    outer->ip_tos = inner->ip_tos; //copy TOS
    //outer->ip_len = size + IPV4_HEADER_SIZE; //whole packet is the inner packet + outer header
    //kernel fills length field anyway
    outer->ip_p = IPV4_HEADER_PROTO_IPIP; //IPIP protocol used
    outer->ip_id = 0; //no fragmentation
    outer->ip_off = inner->ip_off & IP_DF; //copy don't fragment flag only. Set other bits to 0
    outer->ip_ttl = config.ttl; //set TTL
    outer->ip_id = 0; //let kernel fill ID field

    if(config.local.s_addr != 0) //use specified local address?
        outer->ip_src = config.local;
    else
        outer->ip_src.s_addr = 0; //else let kernel fill source IP



    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = ipip_getDestination(inner->ip_dst.s_addr); //get tunnel (outer) destination

    if(dest.sin_addr.s_addr == 0) //do not send when remote address is not known
    {
        PRINT("Unknown remote address!\n");
        //set ICMP destination unreachable - host unknown
        ICMP_send(sockfd, &(buf[IPV4_HEADER_SIZE]), size, (config.local.s_addr == 0) ? 0 : config.local.s_addr,
            ICMP_DEST_UNREACH, ICMP_HOST_UNKNOWN, 0);
        return -1;
    }

    outer->ip_dst.s_addr = dest.sin_addr.s_addr; //fill outer destination field

    if(outer->ip_dst.s_addr == inner->ip_src.s_addr) //drop if tunnel destination is the same as inner packet source (RFC 2003)
    {
        PRINT("Dropping packet: tunnel destination = datagram source\n");
        return -1;
    }
    
    int sent = sendto(sockfd, buf, size + IPV4_HEADER_SIZE, 0, (struct sockaddr*)(&dest), sizeof(dest)); //send encapsulated packet
    
    if(sent < 0) //error
    {
        DEBUG("Encapsulated packet TX failed");
        return -1;
    }
    else if(sent != (size + IPV4_HEADER_SIZE)) //number of bytes actually sent is different than number of bytes to be sent
    {
        PRINT("Encapsulated packet TX problem: %d bytes to send, %d actually sent\n", size + IPV4_HEADER_SIZE, sent);
        return -1;
    }

    return 0;
}

/**
 * @brief Encapsulate IPv6 packet and send as IPv4 packet
 * @param buf Packet buffer with additional space for outer header (inner header must at buf + IPV4_HEADER_SIZE)
 * @param size Size of packet being encapsulated (inner data excluding space left for outer header)
 * @return 0 if success, -1 otherwise
**/
int ip6ip_encap(uint8_t *buf, int size)
{
    PRINT("IPv6 handling not implemented yet\n");
    return -1;
}

/**
 * @brief Decapsulate IPv4 packet and send as IPv4 packet
 * @param buf Encapsulated packet buffer
 * @param size Size of encapsulated packet
 * @return 0 if success, -1 otherwise
**/
int ipip_decap(uint8_t *buf, int size)
{
    if(size < (2 * IPV4_HEADER_SIZE)) //the encapsulated packet must contain at least both headers
    {
        PRINT("Received IPIP-like packet, but it is too short (%d bytes)\n", size);
        return -1;
    }
    
    struct ip *outer = (struct ip*)buf; //set pointer to outer IP header
    struct ip *inner = (struct ip*)(&buf[IPV4_HEADER_SIZE]); //get inner IP header

    if(inner->ip_v != IPVX_HEADER_VERSION_4) //inner packet is not an IPv4 packet
        return -1;

    uint16_t oldSum = outer->ip_sum; //store original checksum
    ipv4_checksum((uint8_t*)outer, outer->ip_hl * 4); //recalculate checksum
    if(oldSum != outer->ip_sum) //checksum does not match
    {
        PRINT("Outer packet checksum check failed\n");
        return -1;
    }

    oldSum = inner->ip_sum; //store original checksum
    ipv4_checksum((uint8_t*)inner, inner->ip_hl * 4); //recalculate checksum
    if(oldSum != inner->ip_sum) //checksum does not match
    {
        PRINT("Inner packet checksum check failed\n");
        return -1;
    }


    if(outer->ip_hl != (IPV4_HEADER_SIZE / 4)) //drop packets than don't have standard headers
    {
        PRINT("Blocking IPv4 packet with header length other than %d bytes.\n", IPV4_HEADER_SIZE);
        return -1;
    }

    if(inner->ip_hl != (IPV4_HEADER_SIZE / 4)) //drop packets than don't have standard headers
    {
        PRINT("Blocking IPv4 packet with header length other than %d bytes.\n", IPV4_HEADER_SIZE);
        return -1;
    }

    if(inner->ip_ttl == 0) //TTL exceeded - drop packet (RFC 2003)
        return 0;

    if(ntohs(inner->ip_len) != (size - IPV4_HEADER_SIZE)) //inner packet has different length than specified in header
    {
        PRINT("Packet length inconsistent (header claims %d bytes, actually has %d bytes)\n", inner->ip_len, size - IPV4_HEADER_SIZE);
        return -1;
    }

    int written = write(tunfd, &(buf[IPV4_HEADER_SIZE]), size - IPV4_HEADER_SIZE); //write to TUN interface without outer header
    
    if(written < 0) //error
    {
        DEBUG("Decapsulated packet write failed");
        return -1;
    }
    else if(written != (size - IPV4_HEADER_SIZE)) //number of bytes actually sent is different than number of bytes to be sent
    {
        PRINT("Decapsulated packet write problem: %d bytes to write, %d actually written\n", size - IPV4_HEADER_SIZE, written);
        return -1;
    }

    return 0;
}

/**
 * @brief Decapsulate IPv4 packet and send as IPv6 packet
 * @param buf Encapsulated packet buffer
 * @param size Size of encapsulated packet
 * @return 0 if success, -1 otherwise
**/
int ip6ip_decap(uint8_t *buf, int size)
{
    PRINT("IPv6 handling not implemented yet\n");
    return -1;
}

int Ipip_exec()
{
    uint8_t buf[IP_MAX_PACKET_SIZE]; //create buffer for packets
    
    while(1)
    {
        int err;
        fd_set rd_set; //read set

        FD_ZERO(&rd_set); //zero out
        FD_SET(tunfd, &rd_set); //set mask on tun descriptor
        FD_SET(sockfd, &rd_set); //set mask on socket descriptor

        err = select(((tunfd > sockfd) ? tunfd : sockfd) + 1, &rd_set, NULL, NULL, NULL); //wait for RX event
        
        if(err < 0) //an error occurred
        {
            if(errno == EINTR) //but this was due to signal
                continue; //continue
            else 
                return -1; //otherwise return
        }

        if(FD_ISSET(sockfd, &rd_set)) //event on socket descriptor?
        {
            int size = recv(sockfd, buf, IP_MAX_PACKET_SIZE, 0); //receive encapsulated packet

            if(size < 0) //an error
            {
                //if((errno != EWOULDBLOCK) && (errno != EAGAIN)) //error other than "would block"
                    DEBUG("Socket RX failed");

                goto continueIPIP; //continue the loop
            }
            else if(size == 0) //no data
            {
                PRINT("There was an RX event, but no data was received\n");
                goto continueIPIP; //continue the loop
            }

            struct ip *outer = (struct ip*)(buf); //get outer IP header

            if(outer->ip_v == IPVX_HEADER_VERSION_4) //is this an IPv4 packet?
            {
                if(outer->ip_p == IPV4_HEADER_PROTO_IPIP) //IPIP encapsulated
                    ipip_decap(buf, size); //decapsulate
                else if(outer->ip_p == IPV4_HEADER_PROTO_IP6IP) //IP6IP encapsulated
                    ip6ip_decap(buf, size); //decapsulate
                else //don't process other protocols
                    goto continueIPIP;
            } //not an IPv4 packet? Should not happen anyway
            //just go further

        }

        continueIPIP:

        if(FD_ISSET(tunfd, &rd_set)) //event on tun descriptor?
        {
            int size = read(tunfd, &(buf[IPV4_HEADER_SIZE]), IP_MAX_PACKET_SIZE - IPV4_HEADER_SIZE); //receive packet and leave room for outer IP header

            if(size < 0) //an error
            {
                DEBUG("Tunnel RX failed");
                continue;
            }
            else if(size == 0) //no data
            {
                PRINT("There was an RX event, but no data was received\n");
                continue;
            }
            
            struct ip *inner = (struct ip*)(&buf[IPV4_HEADER_SIZE]); //get inner IP header (IPv4 temporarily - protocol version is still in the same place)
            if(inner->ip_v == IPVX_HEADER_VERSION_4) //this is an IPv4 packet
                if(config.tun4in4) //if enabled
                    ipip_encap(buf, size); //encapsulate and send
            else if(inner->ip_v == IPVX_HEADER_VERSION_6) //this is an IPv6 packet
                if(config.tun6in4) //if enabled
                    ip6ip_encap(buf, size); //encapsulate and send
            else //non-IP packet
                continue; //do not process it
        }
    }
}