#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct Robj;

struct AVLNode {
    Robj* member;
    double score;

    int height;
    int subtree_size;

    AVLNode* left;
    AVLNode* right;
};

class AVLTree {
private:
    AVLNode* root;

    int get_height(AVLNode* n);
    int get_size(AVLNode* n);
    void update_node(AVLNode* n);
    void destroy_recursive(AVLNode* n);

    int cmp(double a_sc, Robj* a_mem, double b_sc, Robj* b_mem);

    AVLNode* left_rotate(AVLNode* a);
    AVLNode* right_rotate(AVLNode* a);
    AVLNode* balance(AVLNode* n);

    AVLNode* insert_util(AVLNode* root, Robj* member, double score);
    AVLNode* erase_util(AVLNode* root, Robj* member, double score);

    int rank_util(AVLNode* root, Robj* member, double score);

    void range_by_rank_util(AVLNode* root, int start, int end,
                            std::vector<Robj*>& out, int& idx);

public:
    AVLTree();
    ~AVLTree();

    void insert(Robj* member, double score);
    void update(Robj* member, double old_score, double new_score);
    void erase(Robj* member, double score);

    int rank(Robj* member, double score);
    std::vector<Robj*> range(int start, int end);
};
