//
//
//

#ifndef ASCIICOMP_FASTHUFFMAN_H
#define ASCIICOMP_FASTHUFFMAN_H

#include <stdint.h>
#include <stdlib.h>
#include <vector>

// This controls the range of our values and can be used to minimize the statuc arrays
// For instance if you know you are dealing with ASCII (0..127) then set this to 128 and change
// the T_VALUE to int8_t
#define FASTHUFF_VALUE_RANGE 256

class FastHuffman {
public:
    // The 'value' is also used as an index
    // we need the full 8 bit range + one value as 'invalid' (replacement for null)
    // therefore I store them as int16_t
    //
    // Note: If you know you are packing data in a certain range you can cut down memory consumption here..
    //       For instance if the data is known to be ASCII (0..127) you can use an 'int8_t' instead..
    //
    //
    //

    using T_FREQ=uint32_t;
    using T_VALUE=int16_t;
    struct PQItem {
        T_VALUE value;
        T_FREQ freq;
    };
    struct TreeNode {
        T_VALUE idxParent, idxLeft, idxRight;
        T_VALUE value;
        T_FREQ freq;
    };
public:
    FastHuffman();
    ~FastHuffman() = default;
    void CalcHistogram(const uint8_t *data, size_t szData);
    void BuildTree();
    void CompressWithTree(const uint8_t *data, size_t szData);
    void PrintHistogram();
    void PrintPQueue();
    // These are more or less just for debugging...
    std::vector<uint8_t> PathForValue(uint8_t value);
    void PrintPath(uint8_t value, std::vector<uint8_t> &path);
private:
    bool PQIsEmpty();
    void PushPQItem(T_VALUE value, T_FREQ freq);
    PQItem PopPQItem();
    static TreeNode *InitNode(TreeNode *node, T_VALUE value, T_FREQ freq);
private:
    int nItems;
    PQItem items[FASTHUFF_VALUE_RANGE]{};
    // Since we add intermediate nodes to the tree we need a few more...
    // note twice, but n + n*log(n) - however, this is easier.. =)
    TreeNode nodes[FASTHUFF_VALUE_RANGE * 2]{};
    uint32_t histogram[FASTHUFF_VALUE_RANGE]{};
};


#endif //ASCIICOMP_FASTHUFFMAN_H
