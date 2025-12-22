#include "hashmap.h"
#include <cstdint>
#include <cstring>
using namespace std;

static uint64_t hash_helper(const char* key , uint32_t key_len){
    uint64_t h = 1469598103934665603ULL;

    for(int i = 0; i<key_len; i++){
        h^=(uint8_t)key[i];
        h *= 1099511628211ULL;
    }

    return h;
}

uint64_t HashTable::hash(const char* key, uint32_t key_len){
    return hash_helper(key, key_len)%bucket_count;
}

HashTable::HashTable(uint32_t init_buckets){
    bucket_count = init_buckets;
    size = 0;
    table = (HashEntry**)calloc(init_buckets, sizeof(HashEntry*));
}

HashTable::~HashTable(){
    for(int i = 0; i<bucket_count; i++){
        HashEntry* e = table[i];

        while(e){
            HashEntry* next = e->next;
            free(e->key);
            free(e->val);
            free(e);
            e = next;
        }
    }

    free(table);
}

HashEntry* HashTable::find(const char* key, uint32_t key_len){
    uint64_t table_key = hash(key, key_len);

    if(!table[table_key]) return nullptr;

    HashEntry* ptr = table[table_key];

    while(ptr){
        if(ptr->key_len == key_len && memcmp(ptr->key, key, key_len) == 0){
            return ptr;
        }
        ptr = ptr->next;
    }

    return nullptr;
}

bool HashTable::insert(const char* key, uint32_t key_len, const char* val, uint32_t val_len){
    uint64_t table_key = hash(key, key_len);
    
    HashEntry* cur = table[table_key];
    while (cur) {
        if (cur->key_len == key_len &&
            memcmp(cur->key, key, key_len) == 0) {
            free(cur->val);
            cur->val = (char*)malloc(val_len);
            memcpy(cur->val, val, val_len);
            cur->val_len = val_len;
            return true;
        }
        cur = cur->next;
    }

    HashEntry* ptr = table[table_key];

    if(!ptr){
        size++;
        ptr = (HashEntry*)malloc(sizeof(HashEntry));

        if(!ptr){
            return false;
        }

        ptr->key = (char*)malloc(key_len);
        memcpy(ptr->key, key, key_len);
        ptr->key_len = key_len;
        ptr->val = (char*)malloc(val_len);
        memcpy(ptr->val, val, val_len);
        ptr->val_len = val_len;
        ptr->next = nullptr;
        table[table_key] = ptr;
        return true;
    }

    HashEntry* newEntry = (HashEntry*)malloc(sizeof(HashEntry));
    if(!newEntry){
        return false;
    }

    
    newEntry->key = (char*)malloc(key_len*sizeof(char));
    memcpy(newEntry->key, key, key_len);
    newEntry->key_len = key_len;
    newEntry->val = (char*)malloc(val_len*sizeof(char));
    memcpy(newEntry->val, val, val_len);
    newEntry->val_len = val_len;
    newEntry->next = ptr;
    table[table_key] = newEntry;
    size++;
    return true;
}

bool HashTable::erase(const char* key, uint32_t key_len){
    uint64_t table_key = hash(key, key_len);
    HashEntry* prev = nullptr;
    HashEntry* ptr = table[table_key];

    while(ptr){
        if (ptr->key_len == key_len && memcmp(ptr->key, key, key_len) == 0){
            if(prev) prev->next = ptr->next; 
            else table[table_key] = ptr->next;
            
            free(ptr->val);
            free(ptr->key);
            free(ptr);
            size--;

            return true;
        }

        prev = ptr;
        ptr = ptr->next;
    }

    return false;
}

HashEntry* HashTable::bucket_at_idx(uint64_t idx){
    if(idx>=bucket_count) return nullptr;
    return table[idx];
}

void HashTable::insert_entry(HashEntry* e) {
    uint64_t idx = hash(e->key, e->key_len);
    e->next = table[idx];
    table[idx] = e;
    size++;
}


void HashTable::decrement_size(){
    size--;
}

void HashTable::set_null(uint64_t idx){
    table[idx] = nullptr;
}