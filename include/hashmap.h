#pragma once
#include "Robj.h"
#include <cstdint>
#include <cstddef>
#include <string>


struct HashEntry{
    Robj* key;
    Robj* val;
    struct HashEntry* next;
};

class HashTable{
    private:
    HashEntry** table;
    uint32_t size;
    uint32_t bucket_count;

    uint64_t hash(const char* key, uint32_t len);

    public:
    
    HashTable(uint32_t init_buckets);

    ~HashTable();

    void insert_entry(HashEntry* e);

    HashEntry* find(Robj* key);

    bool insert(Robj* key, Robj* val);

    bool erase(Robj* key);

    size_t count(){
        return size;
    }

    void set_null(uint64_t idx);

    uint32_t get_bucket_count(){
        return bucket_count;
    }

    HashEntry* bucket_at_idx(uint64_t idx);
    
    uint32_t get_size();
    void decrement_size();
};