#include "Heap.h"
#include "Robj.h"
#include "Dict.h"
#include "hashmap.h"
#include "Helper.h"
#include <sys/types.h>


Dict::Dict(uint32_t init_buckets){
    ht[0] = new HashTable(init_buckets);
    ht[1] = nullptr;
    heap = new Heap();
    rehash_idx = -1;
}

void Dict::start_rehashing(){
    if (rehash_idx != -1) return;
    ht[1] = new HashTable(ht[0]->get_bucket_count()*2);
    rehash_idx = 0;
}


HashEntry* Dict::find_from(const char* key, uint32_t key_len){

    Robj* key_obj = create_obj(key, key_len, RobjType::OBJ_STRING);
    HashEntry* found = ht[0]->find(key_obj);
    if(!found && ht[1]) found = ht[1]->find(key_obj);
    decr_refcount(key_obj);

    if(found && found->expires_at != 0 && found->expires_at <= now_ns()) {
        erase_from(key, key_len);
        return nullptr;
    }

    return found;
}


bool Dict::insert_into(const char* key, uint32_t key_len, const char* val, uint32_t val_len, uint64_t expiry) {

    Robj* key_obj = create_obj(key, key_len, RobjType::OBJ_STRING);
    Robj* val_obj = create_obj(val, val_len, RobjType::OBJ_STRING);
 
    if (rehash_idx != -1) rehash();
 
    bool success = false;
    if (rehash_idx != -1) { 
        ht[0]->erase(key_obj);
        success = ht[1]->insert(key_obj, val_obj, expiry);
    } else {
        success = ht[0]->insert(key_obj, val_obj, expiry);
    }

    if (success && expiry > 0) {
        heap->push(key_obj, expiry);
    }

    decr_refcount(key_obj);
    decr_refcount(val_obj);
    
    if (should_start_rehashing()) start_rehashing();
    return success;
}

bool Dict::insert_into(const char* key, uint32_t key_len, uint64_t expiry){
    Robj* key_obj = create_obj(key, key_len, RobjType::OBJ_STRING);
    Robj* val_obj = create_zset_obj();

    if (rehash_idx != -1) rehash();
 
    bool success = false;
    if (rehash_idx != -1) { 
        ht[0]->erase(key_obj);
        success = ht[1]->insert(key_obj, val_obj, expiry);
    } else {
        success = ht[0]->insert(key_obj, val_obj, expiry);
    }

    if (success && expiry > 0) {
        heap->push(key_obj, expiry);
    }

    decr_refcount(key_obj);
    decr_refcount(val_obj);
    
    if (should_start_rehashing()) start_rehashing();
    return success;
}

void Dict::set_expiry(const char* key, uint32_t key_len, uint64_t expiry_at_ns) {
    HashEntry* e = find_from(key, key_len);
    if (e) {
        e->expires_at = expiry_at_ns;
        Robj* k = create_obj(key, key_len, RobjType::OBJ_STRING);
        heap->push(k, expiry_at_ns);
        decr_refcount(k);
    }
}

uint64_t Dict::get_next_expiry() {
    HeapItem* item = heap->top();
    return item ? item->expires_at : 0;
}

bool Dict::erase_from(const char* key, uint32_t len){
    if(rehash_idx!=-1){
        rehash();
    }

    Robj* key_obj = create_obj(key, len, RobjType::OBJ_STRING);

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

int Dict::count_keys() {
    vector<string> keys;
    get_all_keys(keys);
    return keys.size();
}

void Dict::rehash() {
    if (rehash_idx == -1) return;
    int steps = 10;
    while(steps-- && (uint32_t)rehash_idx < ht[0]->get_bucket_count()){
        while ((uint32_t)rehash_idx < ht[0]->get_bucket_count() && ht[0]->bucket_at_idx(rehash_idx) == nullptr) {
            rehash_idx++;
        }
        if ((uint32_t)rehash_idx == ht[0]->get_bucket_count()) break;

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

    if ((uint32_t)rehash_idx == ht[0]->get_bucket_count()) {
        delete ht[0];
        ht[0] = ht[1];
        ht[1] = nullptr;
        rehash_idx = -1;
    }
}

bool Dict::should_start_rehashing(){
    return ht[0]->count() >= ht[0]->get_bucket_count();
}

void Dict::get_all_keys(vector<string>& out) {
    if (rehash_idx != -1) {
        for (size_t i = rehash_idx; i < ht[0]->get_bucket_count(); i++) {
            HashEntry* e = ht[0]->bucket_at_idx(i);
            while (e) {
                out.emplace_back((const char*)e->key->ptr, e->key->len);
                e = e->next;
            }
        }
    } else {
        for (size_t i = 0; i < ht[0]->get_bucket_count(); i++) {
            HashEntry* e = ht[0]->bucket_at_idx(i);
            while (e) {
                out.emplace_back((const char*)e->key->ptr, e->key->len);
                e = e->next;
            }
        }
    }

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

int Dict::active_expire() {
    int n_expired = 0;
    uint64_t now = now_ns();
    
    int max_cycles = 100; 

    while (max_cycles--) {
        HeapItem* item = heap->top();
        if (!item || item->expires_at > now) break;

        item = heap->pop();

        HashEntry* e = ht[0]->find(item->key);
        if (!e && ht[1]) e = ht[1]->find(item->key);

        if (e) {
            if (e->expires_at != 0 && e->expires_at <= now) {
                erase_from((const char*)item->key->ptr, item->key->len);
                n_expired++;
            }
        }

        decr_refcount(item->key);
        free(item);
    }

    return n_expired;
}


Dict::~Dict(){
    delete ht[0];
    delete ht[1];
    delete heap;
}

