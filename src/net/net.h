
#if !defined(NET_H)
#define NET_H 

#include <netdb.h>

/*** utils ***/

extern int
sendall(int sockfd, const uint8_t* buf, unsigned len);

extern int
recvall(int sockfd, uint8_t* buf, unsigned len);

extern int
get_socket(const char* ip, const char* port);

extern int
validate_ip(const char* ip);

extern int
validate_port(const char* port);

extern void*
get_in_addr(struct sockaddr* sa);

#endif /* !defined(NET_H) */
