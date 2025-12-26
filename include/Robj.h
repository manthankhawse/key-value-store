#pragma once
#include <cstdint>

enum RobjType{
    OBJ_STRING = 0,
    OBJ_INTEGER = 1,
};

struct Robj{
    uint32_t refcount;
    RobjType type;
    void* ptr;
    uint32_t len;
};

Robj* create_string_obj(const char* data, uint32_t len);
void incr_refcount(Robj* o);
void decr_refcount(Robj* o);
