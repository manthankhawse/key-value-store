#include "Dict.h"
#include "Robj.h"
#include "hashmap.h"
#include <sys/types.h>


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

    Robj* key_obj = create_string_obj(key, key_len);

    HashEntry* found = ht[0]->find(key_obj);

    if(found){
        decr_refcount(key_obj);
        return found;
    }
    
    if(ht[1]){
        found = ht[1]->find(key_obj);
    }

    decr_refcount(key_obj);
    return found;
}

bool Dict::insert_into(const char* key, uint32_t key_len, const char* val, uint32_t val_len) {

    Robj* key_obj = create_string_obj(key, key_len);
    Robj* val_obj = create_string_obj(val, val_len);
 
    if (rehash_idx != -1) {
        rehash();
    }
 
    if (rehash_idx != -1) { 
        ht[0]->erase(key_obj);
 
        bool ok = ht[1]->insert(key_obj, val_obj);
        decr_refcount(key_obj);
        decr_refcount(val_obj);

        return ok;
    }
 
    bool success = ht[0]->insert(key_obj, val_obj);

    decr_refcount(key_obj);
    decr_refcount(val_obj);
    
    if (should_start_rehashing()) {
        start_rehashing();
    }

    return success;
}

bool Dict::erase_from(const char* key, uint32_t len){
    if(rehash_idx!=-1){
        rehash();
    }

    Robj* key_obj = create_string_obj(key, len);

    bool removed = ht[0]->erase(key_obj);

    if(!removed && ht[1]){
        removed = ht[1]->erase(key_obj);
    }

    decr_refcount(key_obj);

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
                out.emplace_back((const char*)e->key->ptr, e->key->len);
                e = e->next;
            }
        }
    } else {
        // No rehash: all keys in ht[0]
        for (size_t i = 0; i < ht[0]->get_bucket_count(); i++) {
            HashEntry* e = ht[0]->bucket_at_idx(i);
            while (e) {
                out.emplace_back((const char*)e->key->ptr, e->key->len);
                e = e->next;
            }
        }
    }

    // 2. Keys already moved to new table
    if (rehash_idx != -1) {
        for (size_t i = 0; i < ht[1]->get_bucket_count(); i++) {
            HashEntry* e = ht[1]->bucket_at_idx(i);
            while (e) {
                out.emplace_back((const char*)e->key->ptr, e->key->len);
                e = e->next;
            }
        }
    }
}


Dict::~Dict(){
    delete ht[0];
    delete ht[1];
}

