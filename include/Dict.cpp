#include "Dict.h"
#include "hashmap.h"
#include <sys/types.h>
#include <unordered_set>


Dict::Dict(uint32_t init_buckets){
    ht[0] = new HashTable(init_buckets);
    ht[1] = nullptr;
    rehash_idx = -1;
}

void Dict::start_rehashing(){
    if (rehash_idx != -1) return;
    ht[1] = new HashTable(ht[0]->get_bucket_count()*2);
    rehash_idx = 0;
}


HashEntry* Dict::find_from(const char* key, uint32_t key_len){

    HashEntry* found = ht[0]->find(key, key_len);

    if(found){
        return found;
    }
    
    if(ht[1]){
        found = ht[1]->find(key, key_len);
    }

    return found;
}

bool Dict::insert_into(const char* key, uint32_t key_len, const char* val, uint32_t val_len) {
 
    if (rehash_idx != -1) {
        rehash();
    }
 
    if (rehash_idx != -1) { 
        ht[0]->erase(key, key_len);
 
        return ht[1]->insert(key, key_len, val, val_len);
    }
 
    bool success = ht[0]->insert(key, key_len, val, val_len);
    
    if (should_start_rehashing()) {
        start_rehashing();
    }

    return success;
}

bool Dict::erase_from(const char* key, uint32_t len){
    if(rehash_idx!=-1){
        rehash();
    }

    bool removed = ht[0]->erase(key, len);
    if(!removed && ht[1]){
        removed = ht[1]->erase(key, len);
    }

    if(rehash_idx==-1 && should_start_rehashing()){
        start_rehashing();
    }

    return removed;
}

void Dict::rehash() {
    if (rehash_idx == -1) return;

    while (rehash_idx < ht[0]->get_bucket_count() &&
           ht[0]->bucket_at_idx(rehash_idx) == nullptr) {
        rehash_idx++;
    }

    if (rehash_idx == ht[0]->get_bucket_count()) {
        delete ht[0];
        ht[0] = ht[1];
        ht[1] = nullptr;
        rehash_idx = -1;
        return;
    }

    HashEntry* e = ht[0]->bucket_at_idx(rehash_idx);
    ht[0]->set_null(rehash_idx);

    while (e) {
        HashEntry* next = e->next;
        e->next = nullptr;          
        ht[1]->insert_entry(e);    
        ht[0]->decrement_size();
        e = next;
    }

    rehash_idx++;
}


bool Dict::should_start_rehashing(){
    return ht[0]->count() >= ht[0]->get_bucket_count();
}

void Dict::get_all_keys(vector<string>& out) {
    // 1. Keys still in old table (not yet rehashed)
    if (rehash_idx != -1) {
        for (size_t i = rehash_idx; i < ht[0]->get_bucket_count(); i++) {
            HashEntry* e = ht[0]->bucket_at_idx(i);
            while (e) {
                out.emplace_back(e->key, e->key_len);
                e = e->next;
            }
        }
    } else {
        // No rehash: all keys in ht[0]
        for (size_t i = 0; i < ht[0]->get_bucket_count(); i++) {
            HashEntry* e = ht[0]->bucket_at_idx(i);
            while (e) {
                out.emplace_back(e->key, e->key_len);
                e = e->next;
            }
        }
    }

    // 2. Keys already moved to new table
    if (rehash_idx != -1) {
        for (size_t i = 0; i < ht[1]->get_bucket_count(); i++) {
            HashEntry* e = ht[1]->bucket_at_idx(i);
            while (e) {
                out.emplace_back(e->key, e->key_len);
                e = e->next;
            }
        }
    }
}


Dict::~Dict(){
    delete ht[0];
    delete ht[1];
}

