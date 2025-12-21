#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#define MAX_EVENTS 10
#define MAX_LEN 4096

using namespace std;

enum ConnectionState { READING, WRITING, CLOSED };

class Server {
private:
    int epoll_fd = -1;
    int listen_fd = -1;
    sockaddr_in addr{};
    epoll_event ev{}, events[MAX_EVENTS];

public:
    int init(uint16_t port) {
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) return -1;

        int val = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) return -1;
        if (listen(listen_fd, SOMAXCONN) < 0) return -1;

        set_non_blocking(listen_fd);

        epoll_fd = epoll_create1(0);
        if (epoll_fd < 0) return -1;

        ev.events = EPOLLIN;
        ev.data.fd = listen_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);
        return 0;
    }

    void acceptClient() {
        while (true) {
            int cfd = accept(listen_fd, nullptr, nullptr);
            if (cfd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                return;
            }

            set_non_blocking(cfd);
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = cfd;
            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cfd, &ev);
        }
    }

    void set_non_blocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    int epollfd() const { return epoll_fd; }
    int fd() const { return listen_fd; }
    epoll_event* get_events() { return events; }
};

class Connection {
private:
    int fd;
    string read_buf;
    string write_buf;
    ConnectionState state = READING;

public:
    explicit Connection(int f) : fd(f) {}

    void on_read(int epfd) {
        char buf[4096];

        // drain socket
        while (true) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                read_buf.append(buf, n);
            } else if (n == 0) {
                state = CLOSED;
                return;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                state = CLOSED;
                return;
            }
        }

        // parse frames
        while (true) {
            if (read_buf.size() < 4) break;

            uint32_t len;
            memcpy(&len, read_buf.data(), 4);

            if (len > MAX_LEN) {
                state = CLOSED;
                return;
            }

            if (read_buf.size() < 4 + len) break;

            string payload = read_buf.substr(4, len);
            read_buf.erase(0, 4 + len);

            cout << "Client said: " << payload << endl;

            // echo response
            write_buf.resize(4 + payload.size());
            memcpy(&write_buf[0], &len, 4);
            memcpy(&write_buf[4], payload.data(), payload.size());

            state = WRITING;

            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.fd = fd;
            epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
        }
    }

    void on_write(int epfd) {
        while (!write_buf.empty()) {
            ssize_t n = write(fd, write_buf.data(), write_buf.size());
            if (n > 0) {
                write_buf.erase(0, n);
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                state = CLOSED;
                return;
            }
        }

        state = READING;

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }

    int handle(int epfd, int events) {
        if (events & (EPOLLERR | EPOLLHUP)) {
            state = CLOSED;
            return -1;
        }

        if (events & EPOLLIN) on_read(epfd);
        if (events & EPOLLOUT) on_write(epfd);

        return state == CLOSED ? -1 : 0;
    }

    static void cleanup(int epfd, Connection* conn, int fd,
                        unordered_map<int, Connection*>& mp) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        delete conn;
        mp.erase(fd);
    }
};

unordered_map<int, Connection*> connection_map;

int main() {
    Server server;
    if (server.init(1234) < 0) return 1;

    while (true) {
        int n = epoll_wait(server.epollfd(),
                           server.get_events(),
                           MAX_EVENTS, -1);

        for (int i = 0; i < n; i++) {
            int fd = server.get_events()[i].data.fd;

            if (fd == server.fd()) {
                server.acceptClient();
            } else {
                if (!connection_map.count(fd))
                    connection_map[fd] = new Connection(fd);

                Connection* c = connection_map[fd];
                if (c->handle(server.epollfd(),
                              server.get_events()[i].events) < 0) {
                    Connection::cleanup(server.epollfd(), c, fd, connection_map);
                }
            }
        }
    }
}
