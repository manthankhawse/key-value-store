#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

struct HashEntry{
    char* key;
    uint32_t key_len;
    char* val;
    uint32_t val_len;
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

    HashEntry* find(const char* key, uint32_t len);

    bool insert(const char* key, uint32_t key_len, const char* val, uint32_t val_len);

    bool erase(const char* key, uint32_t len);

    size_t count(){
        return size;
    }

    void set_null(uint64_t idx);

    uint32_t get_bucket_count(){
        return bucket_count;
    }

    HashEntry* bucket_at_idx(uint64_t idx);

    void decrement_size();
};