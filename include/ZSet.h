#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct Robj;
class Dict;
class AVLTree;

class ZSet {
private:
    Dict* dict;
    AVLTree* tree;

public:
    ZSet();
    ~ZSet();

    bool zadd(const char* member, uint32_t member_len,
              const char* score, uint32_t score_len);

    bool zrem(const char* member, uint32_t member_len);

    int zrank(const char* member, uint32_t member_len);

    std::vector<std::string> zrange(int start, int end);
};
