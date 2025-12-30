# **Mini Redis-like Key-Value Store (C++)**

A high-performance, single-threaded key-value store inspired by Redis — implemented from scratch in modern C++.

This project explores internal systems programming concepts such as:

* Event-driven concurrency using **epoll**
* **Append-Only File (AOF)** based durability
* **Expiry & TTL** semantics with nanosecond precision
* **Sorted Sets (ZSet)** implemented using skip-lists / balanced structure
* **Active expiry scanning**
* **Graceful shutdown with fsync**
* In-memory storage backed by a custom **HashMap + Robj layer**
* **Metrics & INFO command** similar to Redis

> ⚙️ This project is designed as an educational rebuild of Redis internals — validating understanding of event loops, persistence strategies, and datastore semantics.

---

## **Key Features**

### 🧱 Core Key-Value Operations

| Command         | Description                |
| --------------- | -------------------------- |
| `SET key value` | Stores a string value      |
| `GET key`       | Fetches a value or `(nil)` |
| `DELETE key`    | Removes a key              |
| `EXISTS key`    | Returns `1` if present     |

> Values are stored as raw byte buffers via `Robj` — enabling future extension to more data types.

---

### ⏳ Expiry & TTL Support (absolute, relative)

Implements Redis-style expiry:

| Command                     | Description             | Behavior                          |
| --------------------------- | ----------------------- | --------------------------------- |
| `EXPIRE key seconds`        | Set relative expiry     | `(int) 1` on success              |
| `PEXPIREAT key nanoseconds` | Set absolute expiry     | Used internally by AOF            |
| `TTL key`                   | Time-to-live in seconds | `-1` no expiry / `-2` key missing |
| `PERSIST key`               | Remove expiry           | `(int) 1` on success              |

Internally:

* `EXPIRE` → converted into `PEXPIREAT` with `now + seconds`
* Expiry storage done inside `HashEntry.expires_at`
* Expired keys are removed lazily + actively via:

  * `active_expire()` on every event loop tick
  * timeout hints based on next expiry time

This mirrors Redis’ hybrid lazy + active expiration model.

---

### 📚 Append-Only Persistence (AOF)

To ensure durability:

* ALL write commands are appended to `appendonly.aof`
* On restart, the AOF is replayed to reconstruct the in-memory DB
* Flush happens every second via `fdatasync(aof_fd)`

| Command                  | Logged as                 |
| ------------------------ | ------------------------- |
| `SET foo bar`            | `SET foo bar`             |
| `EXPIRE x 2`             | `PEXPIREAT x nanoseconds` |
| `DELETE key`             | `DELETE key`              |
| `ZADD zset score member` | `ZADD zset score member`  |

AOF replay protects correctness across restarts:

```
[Server] Shutting down gracefully...
[Server] AOF closed.
[AOF] replay finished
```

---

### 🏎 Event Loop with Epoll (Non-blocking I/O)

The server uses:

* **edge-triggered EPOLL**
* Non-blocking sockets
* Fixed-size event poll loop

No threads are needed because:

* Only one request is processed at a time per client
* Network concurrency is handled by epoll, not threads

---

### 📊 INFO Metrics

```
> INFO
(info)
# Server
uptime_sec:21
aof_enabled:1

# Stats
total_commands_processed:122
ops_per_sec:17
key_count:6
```

Justification:

* Reinforces observability concepts
* Allows performance introspection similar to Redis

---

### 🔢 Sorted Sets (ZSet)

Supported via `ZADD`, `ZRANK`, `ZRANGE`, `ZREM`.

Example:

```
ZADD scores 100 alice
ZADD scores 200 bob
ZADD scores 50 charlie
ZRANGE scores 0 2
```

Output:

```
(arr) len=3
(str) alice
(str) charlie
(str) bob
(arr) end
```

---

## **Build Instructions**

### **Server**

```bash
g++ -std=c++17 -Wall -Wextra -O2 -pthread -o server server.cpp include/*.cpp
```

### **Client**

```bash
g++ -std=c++17 -Wall -Wextra -O2 -o client client.cpp
```

---

## **Running**

### Start the server

```bash
./server
```

### Run the client

```bash
./client
```

Modify `client.cpp` to test your own workflows.

---

## **Example Session**

```
SET foo bar
GET foo
-> (str) bar

SET temp hello
EXPIRE temp 2
TTL temp
-> (int) 2

sleep...

TTL temp
-> (int) -2    # expired
```

---

## **Directory Layout**

```
include/
  Dict.cpp         # key -> entry mapping
  Heap.cpp         # expiry heap
  Hashmap.cpp
  Robj.cpp         # polymorphic values
  ZSet.cpp         # sorted set implementation
server.cpp         # core event loop & command dispatch
client.cpp         # testing client
appendonly.aof     # persistence log (generated at runtime)
```


---

## **Future Work**

* Snapshotting (RDB)
* Replication
* Multithreading for background tasks
* Partial AOF rewrite (compaction)
* RESP protocol compatibility
* Cluster mode / sharding

---
