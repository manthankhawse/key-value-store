#include "include/Dict.h"
#include "include/hashmap.h"
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <cstring>
#include <fcntl.h>
#include <stdlib.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>

#define MAX_EVENTS 10
#define MAX_LEN 4096

using namespace std;

Dict *dict = new Dict(128);

enum ConnectionState { READING, WRITING, CLOSED };

enum RequestType { GET, SET, DELETE, EXISTS, KEYS, UNKNOWN };

struct parsed_request {
  RequestType type;
  char *key;
  char *value;
};

struct Response {
  string payload;
};

class Server {
private:
  int epoll_fd = -1;
  int listen_fd = -1;
  sockaddr_in addr{};
  epoll_event ev{}, events[MAX_EVENTS];

  static vector<string> split_tokens(const string &s) {
    vector<string> tokens;
    istringstream iss(s);
    string tok;
    while (iss >> tok) {
      tokens.push_back(tok);
    }
    return tokens;
  }

public:
  int init(uint16_t port) {
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
      return -1;

    int val = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
      return -1;
    if (listen(listen_fd, SOMAXCONN) < 0)
      return -1;

    set_non_blocking(listen_fd);

    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
      return -1;

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);
    return 0;
  }

  void acceptClient() {
    while (true) {
      int cfd = accept(listen_fd, nullptr, nullptr);
      if (cfd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          break;
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

  static parsed_request parse_request(const string &payload) {
    parsed_request p{};
    p.type = UNKNOWN;
    p.key = nullptr;
    p.value = nullptr;

    vector<string> tokens = split_tokens(payload);
    if (tokens.empty())
      return p;

    const string &cmd = tokens[0];

    auto alloc_copy = [](const string &s) {
      char *buf = (char *)malloc(s.size());
      memcpy(buf, s.data(), s.size());
      return buf;
    };

    if (cmd == "GET" && tokens.size() == 2) {
      p.type = GET;
      p.key = alloc_copy(tokens[1]);

    } else if (cmd == "SET" && tokens.size() == 3) {
      p.type = SET;
      p.key = alloc_copy(tokens[1]);
      p.value = alloc_copy(tokens[2]);

    } else if (cmd == "DELETE" && tokens.size() == 2) {
      p.type = DELETE;
      p.key = alloc_copy(tokens[1]);

    } else if (cmd == "EXISTS" && tokens.size() == 2) {
      p.type = EXISTS;
      p.key = alloc_copy(tokens[1]);
    } else if (cmd == "KEYS" && tokens.size() == 1) {
      p.type = KEYS;
    }

    return p;
  }

  static Response process_request(parsed_request p) {
    Response r;

    switch (p.type) {

    case GET: {
      HashEntry *e = dict->find_from(p.key, strlen(p.key));
      if (!e) {
        r.payload = ser_nil();
      } else {
        r.payload = ser_str(string(e->val, e->val_len));
      }
      break;
    }

    case SET: {
      dict->insert_into(p.key, strlen(p.key), p.value, strlen(p.value));
      r.payload = ser_nil();
      break;
    }

    case DELETE: {
      bool ok = dict->erase_from(p.key, strlen(p.key));
      r.payload = ser_int(ok ? 1 : 0);
      break;
    }

    case EXISTS: {
      bool ok = dict->find_from(p.key, strlen(p.key)) != nullptr;
      r.payload = ser_int(ok ? 1 : 0);
      break;
    }
    case KEYS: {
      vector<string> keys;
      dict->get_all_keys(keys);
      r.payload = ser_arr(keys);
      break;
    }
    default:
      r.payload = ser_err(1, "Unknown cmd");
    }

    return r;
  }

  static string ser_err(int code, const string &msg) {
    return "(err) " + to_string(code) + " " + msg;
  }

  static string ser_nil() { return "(nil)"; }

  static string ser_str(const string &s) { return "(str) " + s; }

  static string ser_int(int v) { return "(int) " + to_string(v); }

  static string ser_arr(const vector<string> &elems) {
    string out = "(arr) len=" + to_string(elems.size()) + "\n";
    for (auto &e : elems)
      out += "(str) " + e + "\n";
    out += "(arr) end";
    return out;
  }

  int epollfd() const { return epoll_fd; }
  int fd() const { return listen_fd; }
  epoll_event *get_events() { return events; }
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
      if (read_buf.size() < 4)
        break;

      uint32_t len;
      memcpy(&len, read_buf.data(), 4);

      if (len > MAX_LEN) {
        state = CLOSED;
        return;
      }

      if (read_buf.size() < 4 + len)
        break;

      string payload = read_buf.substr(4, len);
      read_buf.erase(0, 4 + len);

      cout << "Client said: " << payload << endl;

      parsed_request p = Server::parse_request(payload);
      Response response = Server::process_request(p);
      string &res = response.payload;
      // send response
      write_buf.resize(4 + res.size());
      len = res.size();
      memcpy(&write_buf[0], &len, 4);
      memcpy(&write_buf[4], res.data(), res.size());

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

    if (events & EPOLLIN)
      on_read(epfd);
    if (events & EPOLLOUT)
      on_write(epfd);

    return state == CLOSED ? -1 : 0;
  }

  static void cleanup(int epfd, Connection *conn, int fd,
                      unordered_map<int, Connection *> &mp) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    delete conn;
    mp.erase(fd);
  }
};

unordered_map<int, Connection *> connection_map;

int main() {
  Server server;
  if (server.init(1234) < 0)
    return 1;

  while (true) {
    int n = epoll_wait(server.epollfd(), server.get_events(), MAX_EVENTS, -1);

    for (int i = 0; i < n; i++) {
      int fd = server.get_events()[i].data.fd;

      if (fd == server.fd()) {
        server.acceptClient();
      } else {
        if (!connection_map.count(fd))
          connection_map[fd] = new Connection(fd);

        Connection *c = connection_map[fd];
        if (c->handle(server.epollfd(), server.get_events()[i].events) < 0) {
          Connection::cleanup(server.epollfd(), c, fd, connection_map);
        }
      }
    }
  }
}
