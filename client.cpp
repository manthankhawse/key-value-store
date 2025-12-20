#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;

class Client {
private:
    int fd = -1;
    sockaddr_in addr{};

public:
    int connect_to_server(const char* ip, uint16_t port) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("socket");
            return -1;
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr.sin_addr);

        if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("connect");
            return -1;
        }

        return 0;
    }

    int send_message(const char* msg) {
        size_t len = strlen(msg);

        ssize_t n = write(fd, msg, len);
        if (n < 0) {
            perror("write");
            return -1;
        }

        char buf[64] = {};
        n = read(fd, buf, sizeof(buf) - 1);
        if (n < 0) {
            perror("read");
            return -1;
        }

        buf[n] = '\0';
        cout << "Server says: " << buf << endl;
        return 0;
    }

    void close_connection() {
        if (fd >= 0) close(fd);
        fd = -1;
    }
};

int main() {
    Client client;

    if (client.connect_to_server("127.0.0.1", 1234) < 0)
        return 1;

    client.send_message("Hello From Client");
    client.close_connection();
    return 0;
}
