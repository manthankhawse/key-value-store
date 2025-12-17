#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

using namespace std;

int process(int connfd) {
    char rbuf[64]{};

    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        perror("read");
        return -1;
    }

    cout << "Client says: " << rbuf << endl;

    const char wbuf[] = "World";
    ssize_t sent = write(connfd, wbuf, sizeof(wbuf) - 1);
    if (sent < 0) {
        perror("write");
        return -1;
    }

    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        perror("setsockopt");
        close(fd);
        return EXIT_FAILURE;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return EXIT_FAILURE;
    }

    if (listen(fd, 16) < 0) {
        perror("listen");
        close(fd);
        return EXIT_FAILURE;
    }

    cout << "Server listening on port 1234" << endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);

        int connfd = accept(fd, (sockaddr*)&client_addr, &len);
        if (connfd < 0) {
            perror("accept");
            continue;
        }

        if (process(connfd) < 0) {
            cerr << "Client processing failed" << endl;
        }

        close(connfd);
    }

    close(fd);
    return EXIT_SUCCESS;
}
