#include "Robj.h"
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

HashEntry* HashTable::find(Robj* key){
    uint64_t table_key = hash((const char*)key->ptr, key->len);

    if(!table[table_key]) return nullptr;

    HashEntry* curr = table[table_key];

    while(curr){
        if(curr->key->len == key->len && memcmp(curr->key->ptr, key->ptr, key->len) == 0){
            return curr;
        }
        curr = curr->next;
    }

    return nullptr;
}

bool HashTable::insert(Robj* key, Robj* val){
    uint64_t table_key = hash((const char*)key->ptr, key->len);
    
    HashEntry* cur = table[table_key];
    while (cur) {
        if (cur->key->len == key->len &&
            memcmp(cur->key->ptr, key->ptr, key->len) == 0) {
            decr_refcount(cur->val);
            incr_refcount(val);
            cur->val = val;
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
        
        incr_refcount(key);
        ptr->key = key;
        incr_refcount(val);
        ptr->val = val;
        ptr->next = nullptr;
        table[table_key] = ptr;
        return true;
    }

    HashEntry* newEntry = (HashEntry*)malloc(sizeof(HashEntry));
    if(!newEntry){
        return false;
    }

    
    incr_refcount(key);
    newEntry->key = key;
    incr_refcount(val);
    newEntry->val = val;
    newEntry->next = ptr;
    table[table_key] = newEntry;
    size++;
    return true;
}

bool HashTable::erase(Robj* key){
    uint64_t table_key = hash((const char*)key->ptr, key->len);
    HashEntry* prev = nullptr;
    HashEntry* ptr = table[table_key];

    while(ptr){
        if (ptr->key->len == key->len && memcmp(ptr->key->ptr, key->ptr, key->len) == 0){
            if(prev) prev->next = ptr->next; 
            else table[table_key] = ptr->next;
            
            decr_refcount(ptr->val);
            decr_refcount(ptr->key);
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
    uint64_t idx = hash((const char*)e->key->ptr, e->key->len);
    e->next = table[idx];
    table[idx] = e;
    size++;
}


void HashTable::decrement_size(){
    size--;
}

uint32_t HashTable::get_size(){
    return size;
}

void HashTable::set_null(uint64_t idx){
    table[idx] = nullptr;
}