#pragma once
#include "Robj.h"
#include <vector>
#include <cstdint>
using namespace std;

struct HeapItem{
    Robj* key;
    uint64_t expires_at;
};

class Heap{
    private:
        vector<HeapItem*> arr;
        void heapify(int idx);
        void swap_items(int i, int j);
    public:
        bool push(Robj* key, uint64_t expires_at);       
        HeapItem* top();               
        HeapItem* pop(); 
        ~Heap();
};