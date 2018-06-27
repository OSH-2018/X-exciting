#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>

#define SERVER_FIFO "/tmp/dht_fifo"
#define CLIENT_FIFO_TEMPLATE "/tmp/dht_client_%d"
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

#ifdef __cplusplus
}
#endif