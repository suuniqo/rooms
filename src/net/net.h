
#if !defined(NET_H)
#define NET_H 

#include <stdint.h>
#include <netdb.h>

/*** communicate ***/

extern int
sendall(int sockfd, const uint8_t* buf, unsigned len);

extern int
recvall(int sockfd, uint8_t* buf, unsigned len);


/*** connect ***/

extern int
get_socket_listen(const char* port);

extern int
get_socket_connect(const char* ip, const char* port);


/*** validate ***/

extern int
validate_ip(const char* ip);

extern int
validate_port(const char* port);


/*** address ***/

extern void*
get_in_addr(struct sockaddr* sa);

#endif /* !defined(NET_H) */
