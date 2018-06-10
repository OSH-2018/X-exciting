#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#define SERVER_FIFO "/tmp/dht.fifo"
struct server_request {
    pid_t id;
    int type;
};

#ifdef __cplusplus
}
#endif