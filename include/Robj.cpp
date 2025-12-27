#include "Robj.h"
#include "ZSet.h"
#include <cstring>
#include <stdlib.h>


Robj* create_obj(const char* data, uint32_t len, RobjType type){
    Robj* o = (Robj*)malloc(sizeof(Robj));
    o->refcount = 1; 
    o->type = type;
    o->len = len;
    o->ptr = malloc(len);
    memcpy(o->ptr, data, len);
    return o;
}

Robj* create_zset_obj(){
    Robj* o = (Robj*)malloc(sizeof(Robj));
    o->refcount = 1;
    o->type = RobjType::OBJ_ZSET;
    ZSet* zset = new ZSet();
    o->ptr = zset;
    return o;
}

void incr_refcount(Robj* o){
    o->refcount++;
}

void decr_refcount(Robj* o){
    if(--o->refcount==0){
        free(o->ptr);
        free(o);
    }
}