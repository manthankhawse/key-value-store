#include <iostream>
#include <assert.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h> 
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
using namespace std;

const size_t max_len = 4096;

static void msg(const char* message){
    cout<<message<<endl;
}

static void die(const char* message){
    int err = errno;
    cout<<"["<<errno<<"] "<<message<<endl;
    abort();
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

static int32_t read_full(int fd, char* buf, int n){
    while(n>0){
        int rv = read(fd, buf, n);
        if(rv<=0){
            return -1;
        }

        n-=rv;
        buf+=rv;
    }

    return 0;
}

static int32_t query(int fd, char* text){
    int len = strlen(text);
    if(len>max_len){
        die("TOO LONG");
    }
    char wbuf[4+max_len];

    memcpy(wbuf, &len, 4);
    memcpy(wbuf+4, text, len);
    int err = write_full(fd, wbuf, 4+len);

    if(err){
        return err;
    }

    char rbuf[4+max_len];
    errno = 0;
    err = read_full(fd, rbuf, 4);

    if (err) {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }


    int length = 0;

    memcpy(&length, rbuf, 4);
    if(length > max_len){
        die("TOO LONG");
        return -1;
    }

    err = read_full(fd, rbuf+4, length);

    if (err) {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }

    rbuf[4 + length] = '\0';

    cout<<"Server :"<<(rbuf+4)<<endl;
    return 0;

}

int main(){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd<0){
        die("SOCKET() error");
    }

    struct sockaddr_in addr{};
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(1234);
    addr.sin_family = AF_INET;

    socklen_t addr_len = sizeof(addr);

    int rv = connect(fd, (struct sockaddr*)&addr, addr_len);

    if(rv){
        die("CONNECT() error");
    }


    int32_t err = query(fd, (char*)"Hello");
    if(err){
        goto L_DONE;
    }

    err = query(fd, (char*)"Hello2");
    if(err){
        goto L_DONE;
    }

    err = query(fd, (char*)"Hello3");
    if(err){
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}
