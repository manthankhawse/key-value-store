#pragma once
#include <vector>
#include <string>
#include "Robj.h"
#include "hashmap.h"
using namespace std;

class Dict{
    private:
        HashTable* ht[2];
        int rehash_idx;

    public:
        Dict(uint32_t init_buckets);
        ~Dict();
        void start_rehashing();
        void rehash();
        void get_all_keys(vector<string>& out);
        bool insert_into(const char* key, uint32_t key_len, const char* val, uint32_t val_len);
        bool insert_into(const char* key, uint32_t key_len);
        bool erase_from(const char* key, uint32_t key_len);
        HashEntry* find_from(const char* key, uint32_t key_len);
        bool should_start_rehashing();
};

