#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <sys/signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <openssl/md5.h>

#include "dht_daemon.h"
#include "dht/dht.h"

#define MAX_BOOTSTRAP_NODES 20
#define MAX_REQUEST 20

#define KNOWN_NODE_FILE ".nodes"
#define ID_FILE ".dht_id"

#define TCP_BUF_LEN 4096
#define INIT 0
#define RECVFILE 1
#define MAX_FILENAME 256
#define UPLOAD_MES_LEN (1 + 20 + 8)
#define DOWNLOAD_MES_LEN (1 + 20)

struct tcp_recv {
    int fd;
    int type;
    char buf[4096];
    int buflen;
    int filefd;
    int filelen;
    char filename[MAX_FILENAME];
    struct tcp_recv *next;
};

struct tcp_send {
    int fd;
    char buf[4096];
    int buflen;
    int offset;
    int filefd;
    struct tcp_send *next;
};

// 最大值、时间，，，
static struct tcp_recv *recv_storage = NULL;
static struct tcp_send *send_storage = NULL;


struct request_storage {
    int clientfd;
    int type;
    struct request_storage *next;
};

#define MAX_FILE_STORAGE 256
static int file_storage_num = 0;
static char path_to_store[] = "./";

static struct request_storage *requests = NULL;
static int request_num = 0;

static struct sockaddr_storage bootstrap_nodes[MAX_BOOTSTRAP_NODES];
static int num_bootstrap_nodes = 0;

static unsigned short myport;
static struct sockaddr myip;

static volatile sig_atomic_t exiting = 0;

static int searching = 0, announce_peer = 0, uploading = 0;


void tcp_recv_store(int fd)
{
    struct tcp_recv *temp;
    temp = (struct tcp_recv *)calloc(1, sizeof(struct tcp_recv));
    temp->fd = fd;
    temp->type = INIT;
    temp->filefd = -1;
    temp->next = recv_storage;
}

struct tcp_recv *get_tcp_recv(int fd)
{
    struct tcp_recv *temp = recv_storage;
    while(temp) {
        if (temp->fd == fd)
            return temp;
        temp = temp->next;
    }
    return NULL;
}

int in_tcp_recv(int fd)
{
    return (get_tcp_recv(fd) != NULL);
}

int remove_tcp_recv(int fd)
{
    struct tcp_recv *temp, *pre;
    temp = pre = recv_storage;
    while(temp) {
        if (temp->fd == fd)
            break;
        pre = temp;
        temp = temp->next;
    }
    if (temp == NULL) {
        return -1;
    }
    close(temp->fd);
    if (temp->filefd >= 0)
        close(temp->filefd);
    if (temp == recv_storage) {
        recv_storage = temp->next;
        free(temp);
    }
    else {
        pre->next = temp->next;
        free(temp);
    }
    return 0;
}

void tcp_send_store(int fd, char *name)
{
    struct tcp_send *temp;
    temp = (struct tcp_send *)calloc(1, sizeof(struct tcp_send));
    temp->fd = fd;
    // open研究。。。。
    temp->filefd = open(name, O_RDONLY);
    temp->next = send_storage;
    send_storage = temp;
}

struct tcp_send *get_tcp_send(int fd)
{
    struct tcp_send *temp = send_storage;
    while(temp) {
        if (temp->fd == fd)
            return temp;
        temp = temp->next;
    }
    return NULL;
}

int in_tcp_send(int fd)
{
    return (get_tcp_send(fd) != NULL);
}

void remove_tcp_send(int fd)
{
    struct tcp_send *temp, *pre;
    temp = pre = send_storage;
    while(temp) {
        if (temp->fd == fd)
            break;
        pre = temp;
        temp = temp->next;
    }
    if (temp == NULL) {
        syslog(LOG_ERR, "remove_tcp_recv : unexpect error");
        exit(EXIT_FAILURE);
    }
    close(temp->fd);
    if (temp->filefd >= 0)
        close(temp->filefd);
    if (temp == send_storage) {
        send_storage = temp->next;
        free(temp);
    }
    else {
        pre->next = temp->next;
        free(temp);
    }
}

void tcp_periodic(fd_set *readfds, fd_set *writefds)
{
    int rc;
    if (recv_storage != NULL) {
        struct tcp_recv *temp = recv_storage;
        while (temp) {           
            switch (temp->type) {
            case INIT:
                if (temp->buflen == 0)
                    break;
                if (temp->buf[0] == 'u') {
                    if (file_storage_num >= MAX_FILE_STORAGE) {
                        FD_CLR(temp->fd, readfds);
                        temp = temp->next;
                        remove_tcp_recv(temp->fd);
                        continue;
                    }
                    if (temp->buflen < UPLOAD_MES_LEN)
                        break;
                    for (int i = 0; i < 20; i++)
                        sprintf(temp->filename + i * 2, "%02x", temp->buf[i + 1]);
                    temp->filename[40] = '\0';
                    sscanf(temp->buf + 21, "%8d", &temp->filelen);
                    char name[MAX_FILENAME];
                    strcpy(name, path_to_store);
                    strcpy(name + strlen(path_to_store), temp->filename);
                    if (access(name, F_OK) == 0) {
                        FD_CLR(temp->fd, readfds);
                        temp = temp->next;
                        remove_tcp_recv(temp->fd);
                        continue;
                    }
                    temp->filefd = open(name, O_WRONLY | O_CREAT);
                    if (temp->filefd < 0) {
                        syslog(LOG_ERR, "create file : %s", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    rc = write(temp->filefd, temp->buf + UPLOAD_MES_LEN, temp->buflen - UPLOAD_MES_LEN);
                    if (rc < 0) {
                        syslog(LOG_ERR, "write file : %s", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    temp->filelen -= rc;
                    temp->buflen = 0;
                    temp->type = RECVFILE;
                }
                else if (temp->buf[0] == 'd') {
                    if (temp->buflen < DOWNLOAD_MES_LEN)
                        break;
                    if (temp->buflen != DOWNLOAD_MES_LEN) {
                        syslog(LOG_INFO, "receive unkown download mes");
                        FD_CLR(temp->fd, readfds);
                        temp = temp->next;
                        remove_tcp_recv(temp->fd);
                        continue;
                    }
                    for (int i = 0; i < 20; i++)
                        sprintf(temp->filename + i * 2, "%02x", temp->buf[i + 1]);
                    temp->filename[40] = '\0';
                    char name[MAX_FILENAME];
                    strcpy(name, path_to_store);
                    strcpy(name + strlen(path_to_store), temp->filename);
                    if (access(name, F_OK) != 0) {
                        syslog(LOG_INFO, "receive unkown download file");
                        FD_CLR(temp->fd, readfds);
                        temp = temp->next;
                        remove_tcp_recv(temp->fd);
                        continue;
                    }
                    tcp_send_store(temp->fd, name);
                    FD_SET(temp->fd, writefds);
                    FD_CLR(temp->fd, readfds);
                    temp = temp->next;
                    remove_tcp_recv(temp->fd);
                    continue;
                }
                else {
                    syslog(LOG_INFO, "receive unkown mes");
                    FD_CLR(temp->fd, readfds);
                    temp = temp->next;
                    remove_tcp_recv(temp->fd);
                    continue;
                }
                break;
            case RECVFILE:
                if (temp->buflen <= 0)
                    break;
                rc = write(temp->filefd, temp->buf, temp->buflen);
                if (rc < 0) {
                    syslog(LOG_ERR, "write file : %s", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                temp->filelen -= rc;
                temp->buflen = 0;
                if (temp->filelen == 0) {
                    unsigned char info_hash[20];
                    for (int i = 0; i < 20; i++)
                        sscanf(temp->filename + i * 2, "%02x", (unsigned int *)&info_hash[i]);
                    dht_storage_store(info_hash, &myip, myport, 1);
                    FD_CLR(temp->fd, readfds);
                    temp = temp->next;
                    remove_tcp_recv(temp->fd);
                }
                break;
            default:
                syslog(LOG_ERR, "unkown tcp recv type");
                exit(EXIT_FAILURE);
            }
            temp = temp->next;
        }
    }
}


void handle_request(struct server_request *req)
{
    char filename[MAX_CLIENT_FIFO_NAME];
    int fd;
    struct server_answer answer;
    struct request_storage *r;
    snprintf(filename, MAX_CLIENT_FIFO_NAME, CLIENT_FIFO_TEMPLATE, req->id);
    fd = open(filename, O_WRONLY);
    if (fd == -1) {
        syslog(LOG_WARNING, "client %d don't open a fifo", req->id);
        return;
    }

    if (request_num >= MAX_REQUEST) {
        syslog(LOG_WARNING, "request too much");
        answer.type = DHT_TOO_MUCH_REQUEST;
        memset(&answer.addr, 0, sizeof(struct sockaddr_storage));
        answer.addr_len = 0;
        write(fd, &answer, sizeof(struct server_answer));
        close(fd);
        return;
    }

    r = (struct request_storage *)malloc(sizeof(struct request_storage));
    if (r == NULL) {
        syslog(LOG_ERR, "can't malloc space for request");
        answer.type = DHT_MEMORY_NOT_ENOUGH;
        memset(&answer.addr, 0, sizeof(struct sockaddr_storage));
        answer.addr_len = 0;
        write(fd, &answer, sizeof(struct server_answer));
        close(fd);
        return;
    }
    
    switch(req->type) {
    case DHT_DOWNLOAD:
        searching = 1;
        break;
    case DHT_UPLOAD:
        uploading = 1;
        break;
    default:
        syslog(LOG_ERR, "unkown request");
        answer.type = DHT_UNKNOWN_REQUEST;
        memset(&answer.addr, 0, sizeof(struct sockaddr_storage));
        answer.addr_len = 0;
        write(fd, &answer, sizeof(struct server_answer));
        close(fd);
        return;
    };

    r->clientfd = fd;
    r->type = req->type;
    r->next = requests;
    requests = r;
    request_num++;
}

static int becomeDaemon(void)
{
    pid_t pid;
    pid = fork();
    if (pid < 0)
        return -1;
    else if (pid > 0)
        exit(EXIT_SUCCESS);
    umask(0);
    close(STDIN_FILENO);
    int fd = open("/dev/null", O_RDWR);
    if (fd != STDIN_FILENO)
        return -1;
    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
        return -1;
    if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
        return -1;
    return 0;
}

static void sigexit(int signo)
{
    exiting = 1;
    closelog();
    dht_uninit();
    exit(EXIT_SUCCESS);
}

static void init_signals(void)
{
    struct sigaction sa;
    int rc;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigexit;
    sa.sa_flags = 0;
    rc = sigaction(SIGINT, &sa, NULL);
    if (rc == -1) {
        syslog(LOG_ERR, "signaction SIGINT: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    rc = sigaction(SIGTERM, &sa, NULL);
    if (rc == -1) {
        syslog(LOG_ERR, "signaction SIGTERM: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/* The call-back function is called by the DHT whenever something
   interesting happens.  Right now, it only happens when we get a new value or
   when a search completes, but this may be extended in future versions. */
static void callback(void *closure,
         int event,
         const unsigned char *hash,
         const void *data, size_t data_len)
{
    struct server_answer ans;
    struct sockaddr_storage st;
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;

    memset(&st, 0, sizeof(st));

    if(event == DHT_EVENT_SEARCH_DONE) {
        ans.type = DHT_DONE;
        write(requests->clientfd, &ans, sizeof(struct server_answer));
        close(requests->clientfd);
        free(requests);
        requests = NULL;
    }
    else if(event == DHT_EVENT_SEARCH_DONE6) {
        ans.type = DHT_DONE;
        write(requests->clientfd, &ans, sizeof(struct server_answer));
        close(requests->clientfd);
        free(requests);
        requests = NULL;
    }
    else if(event == DHT_EVENT_VALUES) {
        struct in_addr addr;
        int16_t port;
        ans.type = DHT_NODE;
        ans.addr_len = sizeof(struct sockaddr_in);

        sin = (struct sockaddr_in *)&st;
        sin->sin_family = AF_INET;
        for (int i = 0; i < data_len / 6; i++) {
            memcpy(&addr, data + i * 6, 4);
            memcpy(&port, data + i * 6 + 4, 2);
            sin->sin_addr = addr;
            sin->sin_port = port;
            ans.addr = st;
            write(requests->clientfd, &ans, sizeof(struct server_answer));
        }
    }
    else if(event == DHT_EVENT_VALUES6) {
        struct in6_addr addr;
        int16_t port;
        ans.type = DHT_NODE;
        ans.addr_len = sizeof(struct sockaddr_in6);

        sin6 = (struct sockaddr_in6 *)&st;
        sin6->sin6_family = AF_INET6;
        for (int i = 0; i < data_len / 18; i++) {
            memcpy(&addr, data + i * 18, 16);
            memcpy(&port, data + i * 18 + 16, 2);
            sin6->sin6_addr = addr;
            sin6->sin6_port = port;
            ans.addr = st;
            write(requests->clientfd, &ans, sizeof(struct server_answer));
        }
    }
    else
        printf("Unknown DHT event %d.\n", event);
}

static unsigned char buf[4096];

static int set_nonblocking(int fd, int nonblocking)
{
    int rc;
    rc = fcntl(fd, F_GETFL, 0);
    if(rc < 0)
        return -1;

    rc = fcntl(fd, F_SETFL,
               nonblocking ? (rc | O_NONBLOCK) : (rc & ~O_NONBLOCK));
    if(rc < 0)
        return -1;

    return 0;
}

int main(int argc, char **argv)
{
    int i, rc, fd;
    int s = -1, s6 = -1;
    uint16_t udp_port = UDP_PORT, tcp_port = TCP_PORT;
    int tcp_s = -1, tcp_s6 = -1;
    int have_id = 0;
    unsigned char myid[20];
    time_t tosleep = 0;
    char id_file[] = ID_FILE;
    int opt;
    int quiet = 0, ipv4 = 1, ipv6 = 0;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
    struct sockaddr_storage from;
    socklen_t fromlen;

    int fifo_fd, extra_fd;
    int maxfd;
    struct server_request req;

    FILE *nodes_file;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;

/*
    while(1) {
        opt = getopt(argc, argv, "q46b:i:");
        if(opt < 0)
            break;

        switch(opt) {
        case 'q': quiet = 1; break;
        case '4': ipv6 = 0; break;
        case '6': ipv4 = 0; break;
        case 'b': {
            char buf[16];
            int rc;
            rc = inet_pton(AF_INET, optarg, buf);
            if(rc == 1) {
                memcpy(&sin.sin_addr, buf, 4);
                break;
            }
            rc = inet_pton(AF_INET6, optarg, buf);
            if(rc == 1) {
                memcpy(&sin6.sin6_addr, buf, 16);
                break;
            }
            goto usage;
        }
            break;
        case 'i':
            id_file = optarg;
            break;
        default:
            goto usage;
        }
    }
*/
    /* Ids need to be distributed evenly, so you cannot just use your
       bittorrent id.  Either generate it randomly, or take the SHA-1 of
       something. */
    fd = open(id_file, O_RDONLY);
    if(fd >= 0) {
        rc = read(fd, myid, 20);
        if(rc == 20)
            have_id = 1;
        close(fd);
    }

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("open(random)");
        exit(EXIT_FAILURE);
    }

    if(!have_id) {
        rc = read(fd, myid, 20);
        if(rc < 0) {
            perror("read(random)");
            exit(EXIT_FAILURE);
        }
        have_id = 1;

        int ofd = open(id_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(ofd >= 0) {
            rc = write(ofd, myid, 20);
            if(rc < 20) {
                unlink(id_file);
                perror("write(id file)");
            }
            close(ofd);
        }
        else {
            perror("open(id file)");
        }
    }

    {
        unsigned seed;
        read(fd, &seed, sizeof(seed));
        srandom(seed);
    }

    close(fd);

/*
    if(argc < 2)
        goto usage;

    i = optind;

    if(argc < i + 1)
        goto usage;

    port = atoi(argv[i++]);
    if(port <= 0 || port >= 0x10000)
        goto usage;
*/

    // 添加初始节点
    FILE *file;
    file = fopen(KNOWN_NODE_FILE, "r");
    // notice
    if (file == NULL) {
        perror("fopen(node file)");
    }
    else {
        while(num_bootstrap_nodes < MAX_BOOTSTRAP_NODES) {
            struct sockaddr_in addr;
            struct sockaddr_in6 addr6;
            uint16_t port;
            char buf[256];
            char *matcha, *matchp;
            
            memset(&addr, 0, sizeof(struct sockaddr_in));
            memset(&addr6, 0, sizeof(struct sockaddr_in6));

            if (fgets(buf, 256, file) == NULL) {
                break;
            }
            matcha = strstr(buf, "addr:");
            matchp = strstr(buf, " port:");

            if ((matcha == NULL) || (matchp == NULL)) {
                break;
            }
            *matchp = '\0';
            if (ipv4) {
                rc = inet_pton(AF_INET, matcha + 5, &addr.sin_addr);
            }
            else if (ipv6) {
                rc = inet_pton(AF_INET6, matcha + 5, &addr6.sin6_addr);
            }
            else
                break;
            if (rc <= 0)
                break;
            rc = sscanf(matchp + 6, "%d", (int *)&port);
            if (rc <= 0) {
                break;
            }

            if (ipv4) {
                addr.sin_port = htons(port);
                addr.sin_family = AF_INET;
                memcpy(&bootstrap_nodes[num_bootstrap_nodes], &addr, sizeof(struct sockaddr_in));
            }
            else {
                addr6.sin6_port = htons(port);
                addr6.sin6_family = AF_INET6;
                memcpy(&bootstrap_nodes[num_bootstrap_nodes], &addr6, sizeof(struct sockaddr_in6));
            }
            num_bootstrap_nodes++;
        }
        fclose(file);
    }

    /* If you set dht_debug to a stream, every action taken by the DHT will
       be logged. */
/*    if(!quiet)
        dht_debug = stdout;
*/
    #ifdef DEBUG
    dht_debug = stdout;
    //dht_debug = fopen("debug", "w");
    // 下面变成daemon
    #else
    rc = becomeDaemon();
    if (rc == -1) {
        perror("becomeDaemon");
        exit(EXIT_FAILURE);
    }
    #endif

    // 打开日志
    openlog(argv[0], LOG_CONS | LOG_PID, LOG_USER);
    syslog(LOG_INFO, "start");

    // 打开节点记录
    nodes_file = fopen(KNOWN_NODE_FILE, "w+");
    if (nodes_file == NULL) {
        syslog(LOG_ERR, "open(node file): %s", strerror(errno));
    }

    /* We need an IPv4 and an IPv6 socket, bound to a stable port.  Rumour
       has it that uTorrent likes you better when it is the same as your
       Bittorrent port. */
    if(ipv4) {
        s = socket(PF_INET, SOCK_DGRAM, 0);
        if(s < 0) {
            syslog(LOG_ERR, "socket(IPv4) : %s", strerror(errno));
        }
        rc = set_nonblocking(s, 1);
        if(rc < 0) {
            syslog(LOG_ERR, "set_nonblocking(IPv4) : %s", strerror(errno));
            syslog(LOG_INFO, "exit");
            exit(EXIT_FAILURE);
        }
    }

    if(ipv6) {
        s6 = socket(PF_INET6, SOCK_DGRAM, 0);
        if(s6 < 0) {
            syslog(LOG_ERR, "socket(IPv6) : %s", strerror(errno));
        }
        rc = set_nonblocking(s6, 1);
        if(rc < 0) {
            syslog(LOG_ERR, "set_nonblocking(IPv6) : %s", strerror(errno));
            syslog(LOG_INFO, "exit");
            exit(EXIT_FAILURE);
        }
    }

    if(s < 0 && s6 < 0) {
        syslog(LOG_ERR, "no valid udp socket");
        exit(EXIT_FAILURE);
    }


    if(s >= 0) {
        sin.sin_port = htons(udp_port);
        rc = bind(s, (struct sockaddr*)&sin, sizeof(sin));
        if(rc < 0) {
            syslog(LOG_ERR, "bind(IPv4) : %s", strerror(errno));
            syslog(LOG_INFO, "exit");
            exit(EXIT_FAILURE);
        }
    }

    if(s6 >= 0) {
        int rc;
        int val = 1;

        rc = setsockopt(s6, IPPROTO_IPV6, IPV6_V6ONLY,
                        (char *)&val, sizeof(val));
        if(rc < 0) {
            syslog(LOG_ERR, "setsockopt(IPV6_V6ONLY) : %s", strerror(errno));
            syslog(LOG_INFO, "exit");
            exit(EXIT_FAILURE);
        }

        /* BEP-32 mandates that we should bind this socket to one of our
           global IPv6 addresses.  In this simple example, this only
           happens if the user used the -b flag. */

        sin6.sin6_port = htons(udp_port);
        rc = bind(s6, (struct sockaddr*)&sin6, sizeof(sin6));
        if(rc < 0) {
            syslog(LOG_ERR, "bind(IPv6) : %s", strerror(errno));
            syslog(LOG_INFO, "exit");
            exit(EXIT_FAILURE);
        }
    }

    /* tcp server */
    if (ipv4) {
        int opt = 1;
        tcp_s = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_s < 0) {
            syslog(LOG_ERR, "socket(tcp IPv4) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        rc = setsockopt(tcp_s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (rc < 0) {
            syslog(LOG_ERR, "setsockopt(tcp IPv4) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        rc = set_nonblocking(tcp_s, 1);
        if(rc < 0) {
            syslog(LOG_ERR, "set_nonblocking(tcp IPv4) : %s", strerror(errno));
            syslog(LOG_INFO, "exit");
            exit(EXIT_FAILURE);
        }

        bzero((char *)&sin, sizeof(struct sockaddr_in));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
        sin.sin_port = htons(tcp_port);
        rc = bind(tcp_s, (struct sockaddr *)&sin, sizeof(sin));
        if (rc < 0) {
            syslog(LOG_ERR, "bind(tcp IPv4) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if (ipv6) {
        int opt = 1;
        tcp_s6 = socket(AF_INET6, SOCK_STREAM, 0);
        if (tcp_s6 < 0) {
            syslog(LOG_ERR, "socket(tcp IPv6) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        rc = set_nonblocking(tcp_s6, 1);
        if(rc < 0) {
            syslog(LOG_ERR, "set_nonblocking(tcp IPv6) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        rc = setsockopt(tcp_s6, IPPROTO_IPV6, IPV6_V6ONLY,
                        (char *)&opt, sizeof(opt));
        if(rc < 0) {
            syslog(LOG_ERR, "setsockopt(tcp IPV6_V6ONLY) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        rc = setsockopt(tcp_s6, IPPROTO_IPV6, SO_REUSEADDR, (char *)&opt, sizeof(opt));
        if (rc < 0) {
            syslog(LOG_ERR, "setsockopt(tcp SO_REUSEADDR) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        bzero((char *)&sin6, sizeof(struct sockaddr_in6));
        sin6.sin6_family = AF_INET6;
        sin6.sin6_addr = in6addr_any;
        sin6.sin6_port = htons(tcp_port);
        rc = bind(tcp_s6, (struct sockaddr *)&sin6, sizeof(sin6));
        if (rc < 0) {
            syslog(LOG_ERR, "bind(tcp IPv6) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if(tcp_s < 0 && tcp_s6 < 0) {
        syslog(LOG_ERR, "no valid tcp socket");
        exit(EXIT_FAILURE);
    }

    if (tcp_s >= 0) {
        rc = listen(tcp_s, 10);
        if (rc < 0) {
            syslog(LOG_ERR, "listen(IPv4) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        socklen_t temp = sizeof(struct sockaddr);
        rc = getsockname(tcp_s, &myip, &temp);
        if (rc < 0) {
            syslog(LOG_ERR, "getsockname(IPv4) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if (tcp_s6 >= 0) {
        rc = listen(tcp_s6, 10);
        if (rc < 0) {
            syslog(LOG_ERR, "listen(IPv6) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        socklen_t temp = sizeof(struct sockaddr);
        rc = getsockname(tcp_s6, &myip, &temp);
        if (rc < 0) {
            syslog(LOG_ERR, "getsockname(IPv6) : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    myport = htons(TCP_PORT);

    /* Init the dht. */
    rc = dht_init(s, s6, myid, (unsigned char*)"JC\0\0");
    if(rc < 0) {
        syslog(LOG_ERR, "dht_init : %s", strerror(errno));
        syslog(LOG_INFO, "exit");
        exit(EXIT_FAILURE);
    }

    // 需要处理退出
    init_signals();

    /* For bootstrapping, we need an initial list of nodes.  This could be
       hard-wired, but can also be obtained from the nodes key of a torrent
       file, or from the PORT bittorrent message.

       Dht_ping_node is the brutal way of bootstrapping -- it actually
       sends a message to the peer.  If you know the nodes' ids, it is
       better to use dht_insert_node. */
    for(i = 0; i < num_bootstrap_nodes; i++) {
        socklen_t salen;
        if(bootstrap_nodes[i].ss_family == AF_INET)
            salen = sizeof(struct sockaddr_in);
        else
            salen = sizeof(struct sockaddr_in6);
        dht_ping_node((struct sockaddr*)&bootstrap_nodes[i], salen);
        /* Don't overload the DHT, or it will drop your nodes. */
        if(i <= 128)
            usleep(random() % 10000);
        else
            usleep(500000 + random() % 400000);
    }

    // open server fifo
    if (access(SERVER_FIFO, F_OK) != 0) {
        rc = mkfifo(SERVER_FIFO, S_IRUSR | S_IWUSR | S_IWGRP);
        if ((rc == -1) && (errno == EEXIST)) {
            // some erro, the file has existed
            syslog(LOG_ERR, "mkfifo : %s", strerror(errno));
            //syslog(LOG_INFO, "if you confirm the file %s is not owned by other application, remove it and restart this daemon", SERVER_FIFO);
            exit(EXIT_FAILURE);
        }
    }
    fifo_fd = open(SERVER_FIFO, O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        syslog(LOG_ERR, "%s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // open an extra fifo, so we never see EOF ????
    extra_fd = open(SERVER_FIFO, O_WRONLY);
    if (extra_fd == -1) {
        syslog(LOG_ERR, "%s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    maxfd = fifo_fd;
    if (s > maxfd)
        maxfd = s;
    if (s6 > maxfd)
        maxfd = s6;
    if (tcp_s > maxfd)
        maxfd = tcp_s;
    if (tcp_s6 > maxfd)
        maxfd = tcp_s6;

    fd_set readfds, writefds;
    FD_ZERO(&writefds);
    FD_ZERO(&readfds);
    if (s >= 0)
        FD_SET(s, &readfds);
    if (s6 >= 0)
        FD_SET(s6, &readfds);
    if (tcp_s >= 0)
        FD_SET(tcp_s, &readfds);
    if (tcp_s6 >= 0)
        FD_SET(tcp_s6, &readfds);
    FD_SET(fifo_fd, &readfds);

    time_t time_dump = random() % 1000000;
    while(1) {
        struct timeval tv;
        fd_set reads, writes;        
        int new_fd;
        int dht_rc = -1;
        int request_rc = -1;
        tv.tv_sec = tosleep;
        tv.tv_usec = random() % 1000000;

        memcpy(&reads, &readfds, sizeof(reads));
        memcpy(&writes, &writefds, sizeof(writes));

        rc = select(maxfd + 1, &reads, &writes, NULL, &tv);
        if (rc < 0) {
            if(errno != EINTR) {
                syslog(LOG_ERR, "select : %s", strerror(errno));
                sleep(1);
            }
        }

        if (exiting)
            break;

        if (rc > 0) {
            fromlen = sizeof(from);
            for (i = 0; i <= maxfd; i++) {
                if (!FD_ISSET(i, &readfds))
                    continue;
                if (i == s) {
                    dht_rc = recvfrom(s, buf, sizeof(buf) - 1, 0,
                              (struct sockaddr*)&from, &fromlen);
                }
                else if (i == s6) {
                    dht_rc = recvfrom(s6, buf, sizeof(buf) - 1, 0,
                              (struct sockaddr*)&from, &fromlen);
                }
                else if (i == fifo_fd) {
                    request_rc = read(fifo_fd, &req, sizeof(struct server_request));
                }
                else if (i == tcp_s) {
                    do {
                        new_fd = accept(tcp_s, NULL, NULL);
                        if (new_fd < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                            syslog(LOG_ERR, "accept : %s", strerror(errno));
                            close(tcp_s);
                            tcp_s = -1;
                        }
                        FD_SET(new_fd, &readfds);
                        if (new_fd > maxfd)
                            maxfd = new_fd;
                        tcp_recv_store(i);
                    } while (new_fd >= 0);
                }
                else if (i == tcp_s6) {
                    do {
                        new_fd = accept(tcp_s6, NULL, NULL);
                        if (new_fd < 0) {
                            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                                syslog(LOG_ERR, "accept(ipv6) : %s", strerror(errno));
                                close(tcp_s6);
                                tcp_s6 = -1;
                            }
                            break;
                        }
                        FD_SET(new_fd, &readfds);
                        if (new_fd > maxfd)
                            maxfd = new_fd;
                        tcp_recv_store(i);
                    } while (new_fd >= 0);
                }
                else {
                    if (in_tcp_recv(i)) {
                        struct tcp_recv *recv_client = get_tcp_recv(i);
                        do {
                            rc = recv(i, recv_client->buf + recv_client->buflen, 
                                TCP_BUF_LEN - recv_client->buflen, 0);
                            if (rc < 0) {
                                if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                                    syslog(LOG_ERR, "recv : %s", strerror(errno));
                                    FD_CLR(i, &readfds);
                                    remove_tcp_recv(i);
                                }
                                else if (errno == EINTR)
                                    continue;
                                break;
                            }
                            if (rc == 0) {
                                FD_CLR(i, &readfds);
                                remove_tcp_recv(i);
                                break;
                            }
                            recv_client->buflen += rc;
                            if (recv_client->buflen == TCP_BUF_LEN) 
                                break;
                        } while (1);
                        /*
                            switch (recv_client->type) {
                            case INIT:
                                if (rc > (INITMESSAGE_LEN - recv_client->buflen)) {
                                    syslog(LOG_ERR, "recv init message too much");
                                    FD_CLR(i, &readfds);
                                    remove_tcp_recv(i);
                                    cont = 0;
                                    break;
                                }
                                memcpy(recv_client->buf + recv_client->buflen, buf, rc);
                                recv_client->buflen += rc;
                                break;
                            case RECVFILE:
                                if (rc > recv_client->filelen) {
                                    syslog(LOG_ERR, "recv file out of range");
                                    close(recv_client->filefd);
                                    unlink(recv_client->filename);
                                    FD_CLR(i, &readfds);
                                    remove_tcp_recv(i);
                                    cont = 0;
                                    break;
                                }
                                write(recv_client->filefd, buf, rc);
                                recv_client->filelen -= rc;
                                break;
                            default:
                                syslog(LOG_ERR, "unkown tcp read type");
                                FD_CLR(i, &readfds);
                                remove_tcp_recv(i);
                                cont = 0;
                                break;
                            }
                        } while (cont);*/
                    }
                    else if (in_tcp_send(i)) {
                        struct tcp_send *send_client = get_tcp_send(i);
                        do {
                            if (send_client->buflen == TCP_BUF_LEN)
                                rc = TCP_BUF_LEN;
                            else {
                                rc = read(send_client->filefd, send_client->buf + send_client->buflen, 
                                    TCP_BUF_LEN - send_client->buflen);
                                send_client->buflen += rc;
                            }    
                            if (rc < 0) {
                                syslog(LOG_ERR, "read file : %s", strerror(errno));
                                FD_CLR(i, &writefds);
                                remove_tcp_send(i);
                                break;
                            }
                            if (rc == 0) {
                                FD_CLR(i, &writefds);
                                remove_tcp_send(i);
                                break;
                            }
                            
                            rc = send(send_client->fd, send_client->buf, send_client->buflen, MSG_NOSIGNAL);
                            if (rc < 0) {
                                if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                                    syslog(LOG_ERR, "recv : %s", strerror(errno));
                                    FD_CLR(i, &writefds);
                                    remove_tcp_send(i);
                                }
                                else if (errno == EINTR)
                                    continue;
                                break;    
                            }
                            send_client->buflen -= rc;
                        } while (1);
                    }
                    else {
                        syslog(LOG_ERR, "select unkown fd");
                        exit(EXIT_FAILURE);
                    }
                }
            } // for (fd)
        } // select rc > 0

        if (dht_rc > 0) {
            buf[dht_rc] = '\0';
            rc = dht_periodic(buf, dht_rc, (struct sockaddr *)&from, fromlen,
                              &tosleep, callback, NULL);
        }
        else {
            rc = dht_periodic(NULL, 0, NULL, 0, &tosleep, callback, NULL);
        }
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                syslog(LOG_ERR, "dht_periodic : %s", strerror(errno));
                if (rc == EINVAL || rc == EFAULT)
                    abort();
                tosleep = 1;
            }
        }

        if (request_rc >= 0) {
            if (request_rc != sizeof(struct server_request)) {
                syslog(LOG_WARNING, "unknown request");
            }
            else
                handle_request(&req);
        }

        tcp_periodic(&readfds, &writefds);

        if (announce_peer) {
            if(s >= 0)
                dht_search(req.info_hash, htons(tcp_port), AF_INET, callback, NULL);
            if(s6 >= 0)
                dht_search(req.info_hash, htons(tcp_port), AF_INET6, callback, NULL);
            announce_peer = 0;
        }

        if (uploading) {
            if(s >= 0)
                dht_search(req.info_hash, 0, AF_INET, callback, NULL);
            if(s6 >= 0)
                dht_search(req.info_hash, 0, AF_INET6, callback, NULL);
            uploading = 0;
        }

        if(searching) {
            if(s >= 0)
                dht_search(req.info_hash, htons(tcp_port), AF_INET, callback, NULL);
            if(s6 >= 0)
                dht_search(req.info_hash, htons(tcp_port), AF_INET6, callback, NULL);
            searching = 0;
        }

        /* node store */
        time_dump -= tosleep;
        if((time_dump < 0) && (nodes_file != NULL)) {
            dht_dump_tables(nodes_file);
            time_dump = random() % 1000000;
        }
    }

    {
        struct sockaddr_in sin[500];
        struct sockaddr_in6 sin6[500];
        int num = 500, num6 = 500;
        int i;
        i = dht_get_nodes(sin, &num, sin6, &num6);
        printf("Found %d (%d + %d) good nodes.\n", i, num, num6);
    }

    close(extra_fd);
    close(fifo_fd);
    return 0;

 usage:
    printf("Usage: dht-example [-q] [-4] [-6] [-i filename] [-b address]...\n"
           "                   port [address port]...\n");
    exit(EXIT_FAILURE);
}


/* Functions called by the DHT. */

int
dht_sendto(int sockfd, const void *buf, int len, int flags,
           const struct sockaddr *to, int tolen)
{
    int rc = sendto(sockfd, buf, len, flags, to, tolen);
    if (rc <= 0) {
        perror("send");
    }
    return rc;
}

int
dht_blacklisted(const struct sockaddr *sa, int salen)
{
    return 0;
}

/* We need to provide a reasonably strong cryptographic hashing function.
   Here's how we'd do it if we had RSA's MD5 code. */
#if 1
void
dht_hash(void *hash_return, int hash_size,
         const void *v1, int len1,
         const void *v2, int len2,
         const void *v3, int len3)
{
    static MD5_CTX ctx;
    uint8_t digest[16];
    MD5_Init(&ctx);
    MD5_Update(&ctx, v1, len1);
    MD5_Update(&ctx, v2, len2);
    MD5_Update(&ctx, v3, len3);
    MD5_Final(digest, &ctx);
    if(hash_size > 16)
        memset((char*)hash_return + 16, 0, hash_size - 16);
    memcpy(hash_return, digest, hash_size > 16 ? 16 : hash_size);
}
#else
/* But for this toy example, we might as well use something weaker. */

void
dht_hash(void *hash_return, int hash_size,
         const void *v1, int len1,
         const void *v2, int len2,
         const void *v3, int len3)
{
    const char *c1 = v1, *c2 = v2, *c3 = v3;
    char key[9];                /* crypt is limited to 8 characters */
    int i;

    memset(key, 0, 9);
#define CRYPT_HAPPY(c) ((c % 0x60) + 0x20)

    for(i = 0; i < 2 && i < len1; i++)
        key[i] = CRYPT_HAPPY(c1[i]);
    for(i = 0; i < 4 && i < len1; i++)
        key[2 + i] = CRYPT_HAPPY(c2[i]);
    for(i = 0; i < 2 && i < len1; i++)
        key[6 + i] = CRYPT_HAPPY(c3[i]);
    strncpy(hash_return, crypt(key, "jc"), hash_size);
}
#endif

int
dht_random_bytes(void *buf, size_t size)
{
    int fd, rc, save;

    fd = open("/dev/urandom", O_RDONLY);
    if(fd < 0)
        return -1;

    rc = read(fd, buf, size);

    save = errno;
    close(fd);
    errno = save;

    return rc;
}
