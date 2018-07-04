#ifndef DHT_DAEMON_
#define DHT_DAEMON_

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERVER_FIFO "dht_fifo"
#define CLIENT_FIFO_TEMPLATE "dht_client_%d"
#define MAX_CLIENT_FIFO_NAME 256
struct server_request {
    pid_t id;
    int type;
    unsigned char info_hash[20];
};

struct server_answer {
    int type;
    struct sockaddr_storage addr;
    int addr_len;
};

/* request type */
#define DHT_DOWNLOAD 0
#define DHT_UPLOAD 1

/* answer type */
#define DHT_UNKNOWN_REQUEST 0
#define DHT_DONE 1
#define DHT_TOO_MUCH_REQUEST 2
#define DHT_MEMORY_NOT_ENOUGH 3
#define DHT_NODE 4

/* port */
#define TCP_PORT 8888
#define UDP_PORT 7777

#ifdef __cplusplus
}
#endif

#endif // DHT_DAEMON_