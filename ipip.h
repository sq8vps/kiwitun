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