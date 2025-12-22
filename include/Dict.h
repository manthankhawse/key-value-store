#pragma once
#include "hashmap.h"

class Dict{
    private:
        HashTable* ht[2];
        int rehash_idx;

    public:
        Dict(uint32_t init_buckets);
        ~Dict();
        void start_rehashing();
        void rehash();
        bool insert_into(HashTable* ht, HashEntry* entry);
        bool insert_into(const char* key, uint32_t key_len, const char* val, uint32_t val_len);
        bool erase_from(const char* key, uint32_t key_len);
        HashEntry* find_from(const char* key, uint32_t key_len);
        bool should_start_rehashing();
};

