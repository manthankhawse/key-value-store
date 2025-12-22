#include <bits/stdc++.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_LEN 4096

using namespace std;

class Client {
private:
    int fd = -1;
    sockaddr_in addr{};

    bool write_full(const void* buf, size_t len) {
        const char* p = static_cast<const char*>(buf);
        while (len > 0) {
            ssize_t n = write(fd, p, len);
            if (n <= 0) {
                perror("write");
                return false;
            }
            p += n;
            len -= n;
        }
        return true;
    }

    bool read_full(void* buf, size_t len) {
        char* p = static_cast<char*>(buf);
        while (len > 0) {
            ssize_t n = read(fd, p, len);
            if (n <= 0) {
                perror("read");
                return false;
            }
            p += n;
            len -= n;
        }
        return true;
    }

public:
    bool connect_to_server(const char* ip, uint16_t port) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("socket");
            return false;
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
            perror("inet_pton");
            return false;
        }

        if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("connect");
            return false;
        }

        return true;
    }

    bool send_message(const string& msg) {
        if (msg.size() > MAX_LEN) {
            cerr << "message too large\n";
            return false;
        }

        uint32_t len = msg.size();

        char wbuf[4 + MAX_LEN];
        memcpy(wbuf, &len, 4);
        memcpy(wbuf + 4, msg.data(), len);

        if (!write_full(wbuf, 4 + len))
            return false;

        uint32_t rlen = 0;
        if (!read_full(&rlen, 4))
            return false;

        if (rlen > MAX_LEN) {
            cerr << "response too large\n";
            return false;
        }

        char rbuf[MAX_LEN + 1];
        if (!read_full(rbuf, rlen))
            return false;

        rbuf[rlen] = '\0';
        cout << "Server says: " << rbuf << endl;
        return true;
    }

    void close_connection() {
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
    }
};

int main() {
    Client client;

    if (!client.connect_to_server("127.0.0.1", 1234))
        return 1;

    client.send_message("SET foo bar");
    client.send_message("GET foo");
    client.send_message("EXISTS foo");
    client.send_message("DELETE foo");
    client.send_message("EXISTS foo");
    client.send_message("GET foo");

    client.send_message("SET moo nar");
    client.send_message("GET moo");
    client.send_message("EXISTS moo");
    client.send_message("DELETE moo");
    client.send_message("EXISTS moo");
    client.send_message("GET moo");

    client.close_connection();
    return 0;
}
