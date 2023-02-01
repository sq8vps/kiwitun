#include "icmp.h"
#include <sys/socket.h>

/**
 * @brief Calculate ICMP packet checksum
 * @param buf Data buffer starting with ICMP header
 * @param size Buffer size in bytes
 * @return 0 if success, -1 otherwise
 * @warning New checksum is inserted into the header.
**/
int icmp_checksum(uint8_t *buf, uint16_t size)
{
    uint32_t sum = 0;
    for(uint16_t i = 0; i < size; i += 2) //group data in 16-bit words
    {
        if(i == ICMP_HEADER_CHECKSUM_POS) //skip checksum field itself (treat is a 0x0000)
            continue;

        sum += (((uint32_t)buf[i] << 8) | (uint32_t)buf[i + 1]); //convert to 16-bit words and add to the sum
    }
    sum = (sum & 0xFFFF) + (sum >> 16); //cut carry bits, shift them and add to the checksum
    if(sum & 0x10000) //check if addition generated carry bit
    {
        sum &= 0xFFFF; //if so, remove it
        sum++; //and add one to the checksum
    }
    sum = ~sum; //bitwise negate (one's complement)
    buf[ICMP_HEADER_CHECKSUM_POS] = (sum >> 8) & 0xFF; //store checksum
    buf[ICMP_HEADER_CHECKSUM_POS + 1] = sum & 0xFF;

    return 0;
}

/**
 * @brief Calculate ICMPv6 packet checksum
 * @param buf Data buffer with ALL data to be send (main IPv6 header, ICMPv6 header and data)
 * @param size Buffer size in bytes
 * @return 0 if success, -1 otherwise
 * @warning New checksum is inserted into the ICMPv6 header.
**/
int icmp_checksum6(uint8_t *buf, uint16_t size)
{
    struct ip6_pseudohdr phdr; //IPv6 pseudo-header
    struct ip6_hdr *hdr = (struct ip6_hdr*)buf; //real IPv6 header

    phdr.ip6_src = hdr->ip6_src; //fill pseudo-header
    phdr.ip6_dst = hdr->ip6_dst;
    phdr.ip6_next = hdr->ip6_nxt;
    phdr.ip6_len = hdr->ip6_plen;
    memset(&(phdr.zeros), 0, sizeof(phdr.zeros));

    uint32_t sum = 0; //calculated checksum

    for(uint8_t i = 0; i < sizeof(phdr); i += 2) //calculate for pseudo IP header
    {
       sum += (((uint32_t)(((uint8_t*)&phdr)[i])) << 8) | (uint32_t)(((uint8_t*)&phdr)[i + 1]); //convert to 16-bit words and add to the sum 
    }

    for(uint16_t i = IPV6_HEADER_SIZE; i < size; i += 2) //group data in 16-bit words (skip real IP header)
    {
        if(i == (ICMP_HEADER_CHECKSUM_POS + IPV6_HEADER_SIZE)) //skip checksum field itself (treat is a 0x0000)
            continue;

        sum += ((((uint32_t)buf[i]) << 8) | (uint32_t)buf[i + 1]); //convert to 16-bit words and add to the sum
    }
    sum = (sum & 0xFFFF) + (sum >> 16); //cut carry bits, shift them and add to the checksum
    if(sum & 0x10000) //check if addition generated carry bit
    {
        sum &= 0xFFFF; //if so, remove it
        sum++; //and add one to the checksum
    }
    sum = ~sum; //bitwise negate (one's complement)
    buf[IPV6_HEADER_SIZE + ICMP_HEADER_CHECKSUM_POS] = (sum >> 8) & 0xFF; //store checksum
    buf[IPV6_HEADER_SIZE + ICMP_HEADER_CHECKSUM_POS + 1] = sum & 0xFF;

    return 0;
}

int ICMP_send(int s, uint8_t *data, int size, in_addr_t source, uint8_t type, uint8_t code, uint32_t rest)
{
    uint8_t buf[2 * IPV4_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE]; //prepare buffer
    if(size < (IPV4_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE)) //check if there is enough data to send ICMP message
    {
        PRINT("Not enough data to send ICMP message\n");
        return -1;
    }
    
    memcpy(buf, data, IPV4_HEADER_SIZE); //copy IP header
    struct ip *hdr = (struct ip*)buf; //get header

    //responding to the sender of the packet than caused an ICMP response
    hdr->ip_dst.s_addr = hdr->ip_src.s_addr; //copy source address to destination address
    hdr->ip_p = IPV4_HEADER_PROTO_ICMP; //ICMP protocol inside
    hdr->ip_id = 0; //let kernel fill ID
    hdr->ip_src.s_addr = source; //store soure IP
    hdr->ip_hl = IPV4_HEADER_SIZE / 4; //to be sure
    hdr->ip_v = IPVX_HEADER_VERSION_4; //to be sure
    hdr->ip_off = 0;
    hdr->ip_tos = 0;
    hdr->ip_ttl = ICMP_DEFAULT_TTL;

    struct icmphdr *icmp = (struct icmphdr*)(&(buf[IPV4_HEADER_SIZE])); //get ICMP header
    icmp->type = type; //set type
    icmp->code = code; //set code
    icmp->un.gateway = rest; //set rest of data
    icmp->checksum = 0; //zero-out before calculation
    memcpy(&(buf[IPV4_HEADER_SIZE + ICMP_HEADER_SIZE]), data, IPV4_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE); //copy original packet header and first 8 bytes
    icmp_checksum(&(buf[IPV4_HEADER_SIZE]), ICMP_HEADER_SIZE + IPV4_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE); //calculate and insert ICMP checksum
    
    struct sockaddr_in dest; //prepare structure for sendto()
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = hdr->ip_dst.s_addr; //copy address

    int sent = sendto(s, buf, 2 * IPV4_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE, 0, (struct sockaddr*)(&dest), sizeof(dest)); //send packet

    if(sent < 0) //error
    {
        DEBUG("ICMP packet TX failed");
        return -1;
    }
    else if(sent != (2 * IPV4_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE)) //number of bytes actually sent is different than number of bytes to be sent
    {
        PRINT("ICMP packet TX problem: %d bytes to send, %d actually sent\n", 2 * IPV4_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE, sent);
        return -1;
    }

    return 0;


}

int ICMP_send6(int s, uint8_t *data, int size, struct in6_addr source, uint8_t type, uint8_t code, uint32_t rest)
{
    uint8_t buf[2 * IPV6_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE]; //prepare buffer
    if(size < (IPV6_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE)) //check if there is enough data to send ICMP message
    {
        PRINT("Not enough data to send ICMP message\n");
        return -1;
    }
    
    memcpy(buf, data, IPV6_HEADER_SIZE); //copy IP header
    struct ip6_hdr *hdr = (struct ip6_hdr*)buf; //get header

    //responding to the sender of the packet than caused an ICMP response
    hdr->ip6_dst = hdr->ip6_src; //copy source address to destination address
    hdr->ip6_ctlun.ip6_un1.ip6_un1_nxt = IPV6_HEADER_PROTO_ICMP6; //ICMPv6 protocol inside
    hdr->ip6_ctlun.ip6_un1.ip6_un1_flow = 0; //reset this field
    hdr->ip6_src = source; //store soure IP
    hdr->ip6_ctlun.ip6_un2_vfc = (IPVX_HEADER_VERSION_6 << 4); //to be sure
    hdr->ip6_ctlun.ip6_un1.ip6_un1_hlim = ICMP_DEFAULT_TTL; //hop limit
    hdr->ip6_ctlun.ip6_un1.ip6_un1_plen = htons(ICMP_HEADER_SIZE + IPV6_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE);

    struct icmp6_hdr *icmp = (struct icmp6_hdr*)(&(buf[IPV6_HEADER_SIZE])); //get ICMP header
    icmp->icmp6_type = type; //set type
    icmp->icmp6_code = code; //set code
    icmp->icmp6_dataun.icmp6_un_data32[0] = rest; //set rest of data
    icmp->icmp6_cksum = 0; //zero-out before calculation
    memcpy(&(buf[IPV6_HEADER_SIZE + ICMP_HEADER_SIZE]), data, IPV6_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE); //copy original packet header and first 8 bytes
    icmp_checksum6(buf, ICMP_HEADER_SIZE + 2 * IPV6_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE); //calculate and insert ICMPv6 checksum
    
    struct sockaddr_in6 dest; //prepare structure for sendto()
    memset(&dest, 0, sizeof(dest));
    dest.sin6_family = AF_INET6;
    dest.sin6_addr = hdr->ip6_dst; //copy address

    int sent = sendto(s, buf, 2 * IPV6_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE, 0, (struct sockaddr*)(&dest), sizeof(dest)); //send packet

    if(sent < 0) //error
    {
        DEBUG("ICMPv6 packet TX failed");
        return -1;
    }
    else if(sent != (2 * IPV6_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE)) //number of bytes actually sent is different than number of bytes to be sent
    {
        PRINT("ICMPv6 packet TX problem: %d bytes to send, %d actually sent\n", 2 * IPV6_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_ADDITIONAL_DATA_SIZE, sent);
        return -1;
    }

    return 0;


}