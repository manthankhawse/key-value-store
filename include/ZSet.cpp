#include "ZSet.h"
#include "AVLTree.h"
#include "Dict.h"
#include "Robj.h"
#include "hashmap.h"
#include <string>
#include <vector>

using namespace std;

ZSet::ZSet(){
    tree = new AVLTree();
    dict = new Dict(128);
}

ZSet::~ZSet(){
    delete tree;
    delete dict;
}

bool ZSet::zadd(const char* member, uint32_t member_len,
                const char* score,  uint32_t score_len)
{
    double new_score = stod(string(score, score_len));
    HashEntry* existing = dict->find_from(member, member_len);

    if (existing) {
        double old_score = stod(string((char*)existing->val->ptr, existing->val->len));
        bool success = dict->insert_into(member, member_len, score, score_len);

        Robj* mem_obj = create_obj(member, member_len, RobjType::OBJ_STRING);
        tree->update(mem_obj, old_score, new_score);

        return success;
    }

    bool success = dict->insert_into(member, member_len, score, score_len);

    Robj* mem_obj = create_obj(member, member_len, RobjType::OBJ_STRING);
    tree->insert(mem_obj, new_score);

    return success;
}

bool ZSet::zrem(const char* member, uint32_t member_len)
{
    HashEntry* e = dict->find_from(member, member_len);
    if (!e) return false;

    double old_score = stod(string((char*)e->val->ptr, e->val->len));

    Robj* mem_obj = create_obj(member, member_len, RobjType::OBJ_STRING);
    tree->erase(mem_obj, old_score);

    return dict->erase_from(member, member_len);
}

int ZSet::zrank(const char* member, uint32_t member_len){
    HashEntry* e = dict->find_from(member, member_len);
    if (!e) return -1;

    double score = stod(string((char*)e->val->ptr, e->val->len));
    Robj* mem_obj = create_obj(member, member_len, RobjType::OBJ_STRING);

    return tree->rank(mem_obj, score);
}

vector<string> ZSet::zrange(int start, int end){
    vector<Robj*> mems = tree->range(start, end);
    vector<string> out;
    for (Robj* m : mems){
        out.emplace_back((char*)m->ptr, m->len);
    }
    return out;
}
