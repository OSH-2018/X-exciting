#include "dht_daemon.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <fcntl.h>

void hash(void *hash_return, int hash_size,
         const void *v, int len)
{
    static MD5_CTX ctx;
    uint8_t digest[16];
    MD5_Init(&ctx);
    MD5_Update(&ctx, v, len);
    MD5_Final(digest, &ctx);
    if(hash_size > 16)
        memset((char*)hash_return + 16, 0, hash_size - 16);
    memcpy(hash_return, digest, hash_size > 16 ? 16 : hash_size);
}

void handle(struct sockaddr_storage *addr, int addr_len, unsigned char *info_hash, char *filename, int i)
{
    char name[strlen(filename) + 4];
    char buf[2048];
    strcpy(name, filename);
    sprintf(name + strlen(filename), "%d", i);
    int fd = open(name, O_WRONLY | O_CREAT);
    if (fd < 0) {
        perror("我不信\n");
        exit(EXIT_FAILURE);
    }
    if (addr_len == sizeof(struct sockaddr_in)) {
        int ss = socket(AF_INET, SOCK_STREAM, 0);
        buf[0] = 'd';
        sprintf(buf + 1, "%s", info_hash);
        if (ss < 0) {
            perror("我不信\n");
            exit(EXIT_FAILURE);
        }
        if (connect(ss, (struct sockaddr *)addr, addr_len) < 0) {
            perror("我不信\n");
            exit(EXIT_FAILURE);
        }
        send(ss, buf, 21, 0);
        while (1) {
            int rc = recv(ss, buf, 2048, 0);
            if (rc == 0) {
                close(fd);
                exit(EXIT_SUCCESS);
            }
            write(fd, buf, rc);
        }
    }
    if (addr_len == sizeof(struct sockaddr_in6)) {
        int ss = socket(AF_INET6, SOCK_STREAM, 0);
        buf[0] = 'd';
        sprintf(buf + 1, "%s", info_hash);
        if (ss < 0) {
            perror("我不信\n");
            exit(EXIT_FAILURE);
        }
        if (connect(ss, (struct sockaddr *)addr, addr_len) < 0) {
            perror("我不信\n");
            exit(EXIT_FAILURE);
        }
        send(ss, buf, 21, 0);
        while (1) {
            int rc = recv(ss, buf, 2048, 0);
            if (rc == 0) {
                close(fd);
                exit(EXIT_SUCCESS);
            }
            write(fd, buf, rc);
        }
    }
}

void upload(struct sockaddr_storage *addr, int addr_len, unsigned char *info_hash, int fd)
{
    char buf[2048];
    int rc;
    struct stat st;
    rc = fstat(fd, &st);
    if (rc < 0) {
        perror("我不信\n");
        exit(EXIT_FAILURE);
    }
    if (addr_len == sizeof(struct sockaddr_in)) {
        int ss = socket(AF_INET, SOCK_STREAM, 0);
        buf[0] = 'u';
        memcpy(buf + 1, info_hash, 20);
        sprintf(buf + 21, "%8d", (int)st.st_size);
        if (ss < 0) {
            perror("我不信\n");
            exit(EXIT_FAILURE);
        }
        if (connect(ss, (struct sockaddr *)addr, addr_len) < 0) {
            perror("我不信\n");
            exit(EXIT_FAILURE);
        }
        send(ss, buf, 1 + 20 + 8, 0);
        while (1) {
            int rc = read(fd, buf, 2048);
            if (rc == 0) {
                close(fd);
                close(ss);
                exit(EXIT_SUCCESS);
            }
            rc = send(ss, buf, 2048, 0);
            if (rc < 0) {
                perror("我不信\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    if (addr_len == sizeof(struct sockaddr_in6)) {
        int ss = socket(AF_INET6, SOCK_STREAM, 0);
        buf[0] = 'u';
        memcpy(buf + 1, info_hash, 20);
        sprintf(buf + 21, "%8d", (int)st.st_size);
        if (ss < 0) {
            perror("我不信\n");
            exit(EXIT_FAILURE);
        }
        if (connect(ss, (struct sockaddr *)addr, addr_len) < 0) {
            perror("我不信\n");
            exit(EXIT_FAILURE);
        }
        send(ss, buf, 1 + 20 + 8, 0);
        while (1) {
            int rc = read(fd, buf, 2048);
            if (rc == 0) {
                close(fd);
                close(ss);
                exit(EXIT_SUCCESS);
            }
            rc = send(ss, buf, 2048, 0);
            if (rc < 0) {
                perror("我不信\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char **argv)
{
    unsigned char info_hash[20];
    struct server_request req;
    struct server_answer ans;
    char fifo_name[MAX_CLIENT_FIFO_NAME];
    int serverfd, clientfd;
    if (argc != 3)
        return 0;
    serverfd = open(SERVER_FIFO, O_WRONLY);
    if (strcmp(argv[1], "get") == 0) {
        hash(info_hash, 20, argv[2], strlen(argv[2]));
        req.id = getpid();
        req.type = DHT_DOWNLOAD;
        memcpy(req.info_hash, info_hash, 20);
        sprintf(fifo_name, CLIENT_FIFO_TEMPLATE, req.id);
        mkfifo(fifo_name, S_IRUSR | S_IWUSR | S_IWGRP);
        clientfd = open(fifo_name, O_RDONLY);
        write(serverfd, &req, sizeof(struct server_request));
        int i = 0;
        while (1) {
            read(clientfd, &ans, sizeof(struct server_answer));
            if (ans.type == DHT_DONE)
                break;
            if (ans.type == DHT_NODE) {
                i++;
                pid_t p = fork();
                if (p == 0) {
                    handle(&ans.addr, ans.addr_len, info_hash, argv[2], i);
                    exit(EXIT_SUCCESS);
                }
                else if (p < 0) {
                    perror("我不信\n");
                    exit(EXIT_FAILURE);
                }
            }
            else
                exit(EXIT_FAILURE);
        }
    }
    if (strcmp(argv[1], "up") == 0) {
        hash(info_hash, 20, argv[2], strlen(argv[2]));
        req.id = getpid();
        req.type = DHT_UPLOAD;
        memcpy(req.info_hash, info_hash, 20);
        sprintf(fifo_name, CLIENT_FIFO_TEMPLATE, req.id);
        mkfifo(fifo_name, S_IRUSR | S_IWUSR | S_IWGRP);
        clientfd = open(fifo_name, O_RDONLY);
        write(serverfd, &req, sizeof(struct server_request));
        int fd = open(argv[2], O_RDONLY);
        if (fd < 0) {
            perror("我不信\n");
            exit(EXIT_FAILURE);
        }
        while (1) {
            read(clientfd, &ans, sizeof(struct server_answer));
            if (ans.type == DHT_DONE)
                break;
            if (ans.type == DHT_NODE) {
                upload(&ans.addr, ans.addr_len, info_hash, fd);
            }
            else
                exit(EXIT_FAILURE);
        }
    }
}