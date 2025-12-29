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
string payload(rbuf, rlen);

cout << "Server says:\n" << payload << endl;

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
    
    cout << "\n--- ZSet Tests ---\n";
    
    client.send_message("ZADD scores 100.5 alice");
    client.send_message("ZADD scores 200.0 bob");
    client.send_message("ZADD scores 50.0 charlie");
    
    cout << "Checking Rank for Alice (expect 1):" << endl;
    client.send_message("ZRANK scores alice");

    cout << "Getting all scores (0 to -1 implies all):" << endl;
    client.send_message("ZRANGE scores 0 5");

    client.send_message("ZREM scores alice");
    
    cout << "Checking Range after Delete:" << endl;
    client.send_message("ZRANGE scores 0 5");

    cout << "--- 1. Basic Expiry Test ---" << endl;
    client.send_message("SET temp_key hello");
    client.send_message("EXPIRE temp_key 1"); // Expire in 1 sec
    client.send_message("TTL temp_key");      // Should be ~1
    sleep(2);
    client.send_message("GET temp_key");      // Should be (nil)

    cout << "--- 2. Testing PERSIST (0 = No Expiry) ---" << endl;
    client.send_message("SET perm_key stable");
    client.send_message("EXPIRE perm_key 2"); // Set to die in 2s
    client.send_message("TTL perm_key");
    
    // SAVE IT! Set expires_at = 0
    client.send_message("PERSIST perm_key"); 
    
    cout << "Sleeping 3 seconds (longer than original TTL)..." << endl;
    sleep(3);
    
    // If logic holds, key should still be here
    client.send_message("TTL perm_key");      // Should be -1 (No Expiry)
    client.send_message("GET perm_key");      // Should be "(str) stable"


    client.close_connection();
    return 0;
}
