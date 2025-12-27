#pragma once
#include "ZSet.h"
#include <cstdint>

enum RobjType{
    OBJ_STRING,
    OBJ_INTEGER,
    OBJ_ZSET
};

struct Robj{
    uint32_t refcount;
    RobjType type;
    void* ptr;
    uint32_t len;
};

Robj* create_obj(const char* data, uint32_t len, RobjType type);
Robj* create_zset_obj();
void incr_refcount(Robj* o);
void decr_refcount(Robj* o);
