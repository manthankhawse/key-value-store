#include "include/Dict.h"
#include "include/Helper.h"
#include "include/Robj.h"
#include "include/ZSet.h"
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

int aof_fd = -1;

bool aof_loading = false;

Dict *dict = new Dict(128);

enum ConnectionState { READING, WRITING, CLOSED };

enum RequestType {
  GET,
  SET,
  DELETE,
  EXISTS,
  KEYS,
  ZADD,
  ZREM,
  ZRANK,
  ZRANGE,
  EXPIRE,
  PERSIST,
  TTL,
  UNKNOWN,
  PEXPIREAT
};

volatile sig_atomic_t g_running = 1;

uint64_t last_fsync = 0;

void signal_handler(int signum) { g_running = 0; }

struct parsed_request {
  RequestType type;
  char *key;
  char *arg1;
  char *arg2;
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
    p.arg1 = nullptr;
    p.arg2 = nullptr;

    vector<string> tokens = split_tokens(payload);
    if (tokens.empty())
      return p;

    const string &cmd = tokens[0];

    auto alloc_copy = [](const string &s) {
      char *buf = (char *)malloc(s.size() + 1);
      memcpy(buf, s.data(), s.size());
      buf[s.size()] = '\0';
      return buf;
    };

    if (cmd == "GET" && tokens.size() == 2) {
      p.type = GET;
      p.key = alloc_copy(tokens[1]);
    } else if (cmd == "SET" && tokens.size() == 3) {
      p.type = SET;
      p.key = alloc_copy(tokens[1]);
      p.arg1 = alloc_copy(tokens[2]);
    } else if (cmd == "DELETE" && tokens.size() == 2) {
      p.type = DELETE;
      p.key = alloc_copy(tokens[1]);
    } else if (cmd == "EXISTS" && tokens.size() == 2) {
      p.type = EXISTS;
      p.key = alloc_copy(tokens[1]);
    } else if (cmd == "KEYS" && tokens.size() == 1) {
      p.type = KEYS;
    } else if (cmd == "EXPIRE" && tokens.size() == 3) {
      p.type = EXPIRE;
      p.key = alloc_copy(tokens[1]);
      p.arg1 = alloc_copy(tokens[2]);
    } else if (cmd == "TTL" && tokens.size() == 2) {
      p.type = TTL;
      p.key = alloc_copy(tokens[1]);
    } else if (cmd == "PERSIST" && tokens.size() == 2) {
      p.type = PERSIST;
      p.key = alloc_copy(tokens[1]);
    } else if (cmd == "PEXPIREAT" && tokens.size() == 3) {
      p.type = PEXPIREAT;
      p.key = alloc_copy(tokens[1]);
      p.arg1 = alloc_copy(tokens[2]);
    } else if (cmd == "ZADD" && tokens.size() == 4) {
      p.type = ZADD;
      p.key = alloc_copy(tokens[1]);
      p.arg1 = alloc_copy(tokens[2]);
      p.arg2 = alloc_copy(tokens[3]);
    } else if (cmd == "ZREM" && tokens.size() == 3) {
      p.type = ZREM;
      p.key = alloc_copy(tokens[1]);
      p.arg1 = alloc_copy(tokens[2]);
    } else if (cmd == "ZRANK" && tokens.size() == 3) {
      p.type = ZRANK;
      p.key = alloc_copy(tokens[1]);
      p.arg1 = alloc_copy(tokens[2]);
    } else if (cmd == "ZRANGE" && tokens.size() == 4) {
      p.type = ZRANGE;
      p.key = alloc_copy(tokens[1]);
      p.arg1 = alloc_copy(tokens[2]);
      p.arg2 = alloc_copy(tokens[3]);
    }

    return p;
  }

  static Response process_request(parsed_request p) {
    Response r;

    auto is_zset = [](HashEntry *e) {
      return e && e->val->type == RobjType::OBJ_ZSET;
    };

    switch (p.type) {

    case GET: {
      HashEntry *e = dict->find_from(p.key, strlen(p.key));
      if (!e) {
        r.payload = ser_nil();
      } else {
        r.payload = ser_str((const char *)e->val->ptr, e->val->len);
      }
      break;
    }

    case SET: {
      dict->insert_into(p.key, strlen(p.key), p.arg1, strlen(p.arg1));
      aof_append("SET " + string(p.key) + " " + p.arg1);
      r.payload = ser_nil();
      break;
    }

    case DELETE: {
      bool ok = dict->erase_from(p.key, strlen(p.key));
      if (ok)
        aof_append("DELETE " + string(p.key));
      r.payload = ser_int(ok ? 1 : 0);
      break;
    }

    case EXPIRE: {
      try {
        uint64_t sec = stoull(p.arg1);
        uint64_t ns_at = now_ns() + (sec * 1000000000ULL);
        dict->set_expiry(p.key, strlen(p.key), ns_at);
        if (!aof_loading) {
          aof_append("PEXPIREAT " + string(p.key) + " " +
                     to_string(ns_at));
        }
        r.payload = ser_int(1);
      } catch (...) {
        r.payload = ser_err(3, "ERR value is not an integer or out of range");
      }
      break;
    }
    case PEXPIREAT: {
      uint64_t ns_at = stoull(p.arg1);
      dict->set_expiry(p.key, strlen(p.key), ns_at);

      if (!aof_loading) {
        aof_append("PEXPIREAT " + string(p.key) + " " + p.arg1);
      }

      r.payload = ser_int(1);
      break;
    }

    case TTL: {
      HashEntry *e = dict->find_from(p.key, strlen(p.key));
      if (!e) {
        r.payload = ser_int(-2);
      } else if (e->expires_at == 0) {
        r.payload = ser_int(-1);
      } else {
        uint64_t now = now_ns();
        if (now >= e->expires_at) {
          r.payload = ser_int(-2);
        } else {
          long long remaining = (e->expires_at - now) / 1000000000ULL;
          r.payload = ser_int(remaining);
        }
      }
      break;
    }

    case PERSIST: {
      HashEntry *e = dict->find_from(p.key, strlen(p.key));
      if (!e) {
        r.payload = ser_int(0);
      } else if (e->expires_at == 0) {
        r.payload = ser_int(0);
      } else {
        dict->set_expiry(p.key, strlen(p.key), 0);
        if (!aof_loading) {
          aof_append("PERSIST " + string(p.key));
        }
        r.payload = ser_int(1);
      }
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
    case ZADD: {
      HashEntry *e = dict->find_from(p.key, strlen(p.key));
      if (!e) {
        dict->insert_into(p.key, strlen(p.key));
        e = dict->find_from(p.key, strlen(p.key));
      }

      if (!is_zset(e)) {
        r.payload = ser_err(2, "WRONGTYPE Operation against a key holding the "
                               "wrong kind of value");
      } else {
        ZSet *zset = (ZSet *)e->val->ptr;
        bool new_elem =
            zset->zadd(p.arg2, strlen(p.arg2), p.arg1, strlen(p.arg1));
        if (new_elem)
          aof_append("ZADD " + string(p.key) + " " + p.arg1 + " " + p.arg2);
        r.payload = ser_int(new_elem ? 1 : 0);
      }
      break;
    }

    case ZREM: {
      HashEntry *e = dict->find_from(p.key, strlen(p.key));
      if (!e) {
        r.payload = ser_int(0);
      } else if (!is_zset(e)) {
        r.payload = ser_err(2, "WRONGTYPE");
      } else {
        ZSet *zset = (ZSet *)e->val->ptr;
        bool removed = zset->zrem(p.arg1, strlen(p.arg1));
        r.payload = ser_int(removed ? 1 : 0);
      }
      break;
    }

    case ZRANK: {
      HashEntry *e = dict->find_from(p.key, strlen(p.key));
      if (!e) {
        r.payload = ser_nil();
      } else if (!is_zset(e)) {
        r.payload = ser_err(2, "WRONGTYPE");
      } else {
        ZSet *zset = (ZSet *)e->val->ptr;
        int rank = zset->zrank(p.arg1, strlen(p.arg1));
        if (rank == -1)
          r.payload = ser_nil();
        else
          r.payload = ser_int(rank);
      }
      break;
    }

    case ZRANGE: {
      HashEntry *e = dict->find_from(p.key, strlen(p.key));
      if (!e) {
        r.payload = ser_nil();
      } else if (!is_zset(e)) {
        r.payload = ser_err(2, "WRONGTYPE");
      } else {
        ZSet *zset = (ZSet *)e->val->ptr;
        int start = atoi(p.arg1);
        int end = atoi(p.arg2);
        vector<string> res = zset->zrange(start, end);
        r.payload = ser_arr(res);
      }
      break;
    }
    default:
      r.payload = ser_err(1, "Unknown cmd");
    }

    if (p.key)
      free(p.key);
    if (p.arg1)
      free(p.arg1);
    if (p.arg2)
      free(p.arg2);

    return r;
  }

  static string ser_err(int code, const string &msg) {
    return "(err) " + to_string(code) + " " + msg;
  }

  static string ser_nil() { return "(nil)"; }

  static string ser_str(const char *buf, uint32_t len) {
    string out = "(str) ";
    out.append(buf, len);
    return out;
  }

  static string ser_int(int v) { return "(int) " + to_string(v); }

  static string ser_arr(const vector<string> &elems) {
    string out = "(arr) len=" + to_string(elems.size()) + "\n";
    for (auto &e : elems) {
      out += ser_str(e.data(), e.size()) + "\n";
    }
    out += "(arr) end";
    return out;
  }

  static void aof_append(const string &raw_cmd) {
    if (aof_loading)
      return;
    if (aof_fd < 0)
      return;
    string line = raw_cmd + "\n";
    write(aof_fd, line.data(), line.size());
  }

  static void aof_replay() {
    aof_loading = true;
    ifstream in("appendonly.aof");
    if (!in.is_open()) {
      cerr << "[AOF] no file found, skipping replay\n";
      return;
    }

    string line;
    while (getline(in, line)) {
      if (line.empty())
        continue;

      parsed_request p = Server::parse_request(line);

      if (p.type == UNKNOWN) {
        cerr << "[AOF] ignoring unknown command: " << line << "\n";
        continue;
      }

      Response r = Server::process_request(p);
    }

    cerr << "[AOF] replay finished\n";
    aof_loading = false;
  }

  void shutdown() {
    if (listen_fd != -1) {
      close(listen_fd);
      listen_fd = -1;
    }
    if (epoll_fd != -1) {
      close(epoll_fd);
      epoll_fd = -1;
    }
    cout << "[Server] Stopped listening." << endl;
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
      len = res.size();
      write_buf.resize(4 + len);
      memcpy(&write_buf[0], &len, 4);
      memcpy(&write_buf[4], res.data(), len);

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
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  Server server;
  aof_fd = open("appendonly.aof", O_CREAT | O_WRONLY | O_APPEND, 0644);
  if (aof_fd < 0) {
    perror("open AOF");
    return 1;
  }

  Server::aof_replay();

  if (server.init(1234) < 0)
    return 1;

  while (g_running) {
    dict->active_expire();

    uint64_t now = now_ns();
    if (aof_fd != -1 && (now - last_fsync) >= 1000000000ULL) {
      fdatasync(aof_fd);
      last_fsync = now;
    }

    int timeout = -1;
    uint64_t next_expiry = dict->get_next_expiry();
    if (next_expiry > 0) {
      uint64_t now = now_ns();
      if (next_expiry <= now)
        timeout = 0;
      else
        timeout = (next_expiry - now) / 1000000;
    }

    if (timeout == -1 || timeout > 100)
      timeout = 100;
    int n =
        epoll_wait(server.epollfd(), server.get_events(), MAX_EVENTS, timeout);

    if (n < 0 && errno == EINTR) {
      continue; // Check g_running loop condition
    }

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

  cout << "\n[Server] Shutting down gracefully..." << endl;

  server.shutdown();

  if (aof_fd != -1) {
    fdatasync(aof_fd);
    close(aof_fd);
    cout << "[Server] AOF closed." << endl;
  }

  for (auto &pair : connection_map) {
    close(pair.first);
    delete pair.second;
  }
  connection_map.clear();
  cout << "[Server] Clients disconnected." << endl;

  delete dict;
  cout << "[Server] Memory freed. Bye!" << endl;

  return 0;
}
