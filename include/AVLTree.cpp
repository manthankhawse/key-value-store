#include "AVLTree.h"
#include "Robj.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

using namespace std;

AVLTree::AVLTree() : root(nullptr) {}

void AVLTree::destroy_recursive(AVLNode* n) {
    if (!n) return;
    destroy_recursive(n->left);
    destroy_recursive(n->right);
    
    if (n->member) {
        decr_refcount(n->member);
    }
    free(n);
}

AVLTree::~AVLTree() {
    destroy_recursive(root);
}

int AVLTree::get_height(AVLNode* n){
    return n ? n->height : 0;
}

int AVLTree::get_size(AVLNode* n){
    return n ? n->subtree_size : 0;
}

void AVLTree::update_node(AVLNode* n){
    if (!n) return;
    n->height       = 1 + max(get_height(n->left), get_height(n->right));
    n->subtree_size = 1 + get_size(n->left) + get_size(n->right);
}


int AVLTree::cmp(double a_sc, Robj* a_mem, double b_sc, Robj* b_mem){
    if (a_sc < b_sc) return -1;
    if (a_sc > b_sc) return 1;

    int m = min(a_mem->len, b_mem->len);
    int r = memcmp(a_mem->ptr, b_mem->ptr, m);
    if (r != 0) return r;

    if (a_mem->len < b_mem->len) return -1;
    if (a_mem->len > b_mem->len) return 1;
    return 0;
}


AVLNode* AVLTree::left_rotate(AVLNode* a){
    AVLNode* b = a->right;
    a->right = b->left;
    b->left = a;

    update_node(a);
    update_node(b);
    return b;
}

AVLNode* AVLTree::right_rotate(AVLNode* a){
    AVLNode* b = a->left;
    a->left = b->right;
    b->right = a;

    update_node(a);
    update_node(b);
    return b;
}


AVLNode* AVLTree::balance(AVLNode* n){
    if (!n) return n;

    int bf = get_height(n->left) - get_height(n->right);

    if (bf > 1){
        if (get_height(n->left->right) > get_height(n->left->left))
            n->left = left_rotate(n->left);
        return right_rotate(n);
    }

    if (bf < -1){
        if (get_height(n->right->left) > get_height(n->right->right))
            n->right = right_rotate(n->right);
        return left_rotate(n);
    }

    return n;
}


AVLNode* AVLTree::insert_util(AVLNode* node, Robj* member, double score){
    if (!node){
        AVLNode* n = (AVLNode*)malloc(sizeof(AVLNode));
        n->member = member;
        n->score = score;
        n->left = n->right = nullptr;
        n->height = 1;
        n->subtree_size = 1;
        return n;
    }

    int c = cmp(score, member, node->score, node->member);
    if (c < 0)       node->left  = insert_util(node->left, member, score);
    else             node->right = insert_util(node->right, member, score);

    update_node(node);
    return balance(node);
}

void AVLTree::insert(Robj* member, double score){
    root = insert_util(root, member, score);
}


AVLNode* AVLTree::erase_util(AVLNode* node, Robj* member, double score){
    if (!node) return nullptr;

    int c = cmp(score, member, node->score, node->member);

    if (c < 0){
        node->left = erase_util(node->left, member, score);
    }
    else if (c > 0){
        node->right = erase_util(node->right, member, score);
    }
    else {
        if (!node->left){
            AVLNode* r = node->right;
            free(node);
            return r;
        }
        if (!node->right){
            AVLNode* l = node->left;
            free(node);
            return l;
        }

        AVLNode* s = node->right;
        while (s->left) s = s->left;

        node->score = s->score;
        node->member = s->member;
        node->right = erase_util(node->right, s->member, s->score);
    }

    update_node(node);
    return balance(node);
}

void AVLTree::erase(Robj* member, double score){
    root = erase_util(root, member, score);
}


void AVLTree::update(Robj* member, double old_score, double new_score){
    erase(member, old_score);
    insert(member, new_score);
}


int AVLTree::rank_util(AVLNode* node, Robj* mem, double score){
    if (!node) return -1;

    int c = cmp(score, mem, node->score, node->member);
    if (c == 0){
        return get_size(node->left);
    }
    else if (c < 0){
        return rank_util(node->left, mem, score);
    }
    else {
        int r = rank_util(node->right, mem, score);
        if (r == -1) return -1;
        return get_size(node->left) + 1 + r;
    }
}

int AVLTree::rank(Robj* member, double score){
    return rank_util(root, member, score);
}


void AVLTree::range_by_rank_util(AVLNode* node, int start, int end,
                                 vector<Robj*>& out, int& idx)
{
    if (!node) return;

    range_by_rank_util(node->left, start, end, out, idx);

    if (idx >= start && idx <= end){
        out.push_back(node->member);
    }
    idx++;

    if (idx > end) return;

    range_by_rank_util(node->right, start, end, out, idx);
}

vector<Robj*> AVLTree::range(int start, int end){
    vector<Robj*> out;
    int idx = 0;
    range_by_rank_util(root, start, end, out, idx);
    return out;
}
