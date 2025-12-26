#include "include/Dict.h"
#include "include/Robj.h"
#include <bits/stdc++.h>
using namespace std;

static string rand_key(int i) {
    return "key_" + to_string(i);
}

static string rand_val() {
    int x = rand() % 1000000;
    return "val_" + to_string(x);
}

int main() {
    srand(time(nullptr));

    Dict dict(8);
    unordered_map<string, string> ref;

    const int OPS = 200000;

    for (int i = 0; i < OPS; i++) {
        int op = rand() % 3;

        string key = rand_key(rand() % 5000);

        if (op == 0) {  // SET
            string val = rand_val();
            dict.insert_into(key.c_str(), key.size(),
                             val.c_str(), val.size());
            ref[key] = val;
        }
        else if (op == 1) {  // GET
            HashEntry* e = dict.find_from(key.c_str(), key.size());
            auto it = ref.find(key);

            if (it == ref.end()) {
                if (e != nullptr) {
                    cerr << "ERROR: found non-existent key " << key << endl;
                    return 1;
                }
            } else {
                if (!e) {
                    cerr << "ERROR: missing key " << key << endl;
                    return 1;
                }
                if (string((const char*)e->val->ptr, e->val->len) != it->second) {
                    cerr << "ERROR: value mismatch for " << key << endl;
                    return 1;
                }
            }
        }
        else {  // DELETE
            dict.erase_from(key.c_str(), key.size());
            ref.erase(key);
        }

        if (i % 20000 == 0) {
            cout << "OK @ op " << i << endl;
        }
    }

    // final verification
    for (auto& [k, v] : ref) {
        HashEntry* e = dict.find_from(k.c_str(), k.size());
        if (!e || string((const char*)e->val->ptr, e->val->len) != v) {
            cerr << "FINAL CHECK FAILED for key " << k << endl;
            return 1;
        }
    }

    cout << "STRESS TEST PASSED ✔" << endl;
    return 0;
}
