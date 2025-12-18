#include <asm-generic/socket.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
using namespace std;

static void msg(const char* message){
    cout<<message<<endl;
}

static void die(const char* message){
    int err = errno;
    cout<<"["<<errno<<"] "<<message<<endl;
    abort();
}

const size_t max_len = 4096;

static int32_t read_full(int fd, char* buf, size_t n){
    while(n>0){
        ssize_t rv = read(fd, buf, n);
        if(rv<=0){
            return -1;
        }

        assert(rv<=n);
        n-=rv;
        buf+=rv;
    }

    return 0;
}

static int32_t write_full(int fd, char* buf, size_t n){
    while(n>0){
        int rv = write(fd, buf, n);
        if(rv<=0){
            return -1;
        }

        n-=rv;
        buf+=rv;
    }

    return 0;
}

static int32_t one_request(int connfd){
    char rbuf[4+max_len+1];

    errno = 0;

    int32_t err = read_full(connfd, rbuf, 4);

    if(err){
        msg(errno==0 ? "EOF" : "read() error");
        return err;
    }

    int len = 0;

    memcpy(&len, rbuf, 4);

    if(len>max_len){
        msg("too long");
        return -1;
    }

    err = read_full(connfd, rbuf+4, len);

    if(err){
        msg("read() error");
        return -1;
    }

    rbuf[4 + len] = '\0';

    cout<<"Client says: "<<(rbuf+4)<<endl;

    const char reply[] = "world";

    char wbuf[4+sizeof(reply)];

    len = strlen(reply);

    memcpy(wbuf, &len, 4);
    memcpy(wbuf+4, reply, len);


    return write_full(connfd, wbuf, 4+len);
}

int main(){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd<0){
        die("SOCKET() error");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_ANY);

    int rv = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if(rv){
        die("BIND() error");
    }

    rv = listen(fd, SOMAXCONN);

    if(rv){
        die("LISTEN() error");
    }

    while(true){
        struct sockaddr_in client_addr{};
        size_t addr_len = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr*)&client_addr, (socklen_t*)&addr_len);

        if(connfd < 0){
            continue;
        }

        while (true) {
            int32_t err = one_request(connfd);
            if (err) {
                break;
            }
        }

        close(connfd);
    }

    return 0;
}