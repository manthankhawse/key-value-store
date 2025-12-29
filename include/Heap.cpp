#include "Heap.h"
#include "Robj.h"
#include <cstdlib>

void Heap::swap_items(int i, int j) {
    HeapItem* temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
}

bool Heap::push(Robj* key, uint64_t expires_at){
    if(!key || expires_at == 0) return false;

    HeapItem* item = (HeapItem*)malloc(sizeof(HeapItem));
    if(!item) return false;

    incr_refcount(key);
    item->key = key;
    item->expires_at = expires_at;

    arr.push_back(item);

    int i = arr.size()-1;
    while(i != 0 && arr[(i-1)/2]->expires_at > arr[i]->expires_at){
        swap_items(i, (i-1)/2);
        i = (i-1)/2;
    }
    return true;
}

HeapItem* Heap::top(){
    if(arr.empty()) return nullptr; // <--- FIXED: Return null if empty
    return arr[0];
} 

HeapItem* Heap::pop(){
    if(arr.empty()) return nullptr; // <--- FIXED: Return null if empty

    swap_items(0, arr.size()-1);
    HeapItem* ret = arr.back();
    arr.pop_back();
    
    if (!arr.empty()) {
        heapify(0);
    }
    return ret;
}

void Heap::heapify(int idx){
    int smallest = idx;
    int left = 2*idx + 1;
    int right = 2*idx + 2;

    if(left < (int)arr.size() && arr[smallest]->expires_at > arr[left]->expires_at){
        smallest = left;
    }

    if(right < (int)arr.size() && arr[smallest]->expires_at > arr[right]->expires_at){
        smallest = right;
    }

    if(smallest != idx){
        swap_items(idx, smallest);
        heapify(smallest);
    }
}

Heap::~Heap(){
    for(auto item : arr){
        decr_refcount(item->key);
        free(item);
    }
}