#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;

class Server {
private:
    int listen_fd = -1;
    sockaddr_in addr{};

public:
    int init(uint16_t port) {
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            perror("socket");
            return -1;
        }

        int val = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            return -1;
        }

        if (listen(listen_fd, SOMAXCONN) < 0) {
            perror("listen");
            return -1;
        }

        return 0;
    }

    int fd() const {
        return listen_fd;
    }
};

class Connection {
private:
    int fd;

public:
    explicit Connection(int fd_) : fd(fd_) {}

    int handle() {
        char buf[64] = {};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0)
            return -1;

        cout << "client says: " << buf << endl;

        const char reply[] = "world";
        write(fd, reply, strlen(reply));
        return 0;
    }
};

int main() {
    Server server;
    if (server.init(1234) < 0)
        return 1;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);

        int connfd = accept(server.fd(),
                            (sockaddr*)&client_addr,
                            &len);

        if (connfd < 0) {
            perror("accept");
            continue;
        }

        Connection conn(connfd);
        conn.handle();
        close(connfd);
    }

    return 0;
}
