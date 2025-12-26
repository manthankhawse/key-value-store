#include "Robj.h"
#include <cstring>
#include <stdlib.h>


Robj* create_string_obj(const char* data, uint32_t len){
    Robj* o = (Robj*)malloc(sizeof(Robj));
    o->type = RobjType::OBJ_STRING;
    o->len = len;
    o->ptr = malloc(len);
    memcpy(o->ptr, data, len);
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