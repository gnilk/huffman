//
// This is not (as the name implies) a fast version of huffman encoding
// Instead it is a zero allocation version for embedded..
//
// Suggestion: replace the static arrays with a buffers from pre-heap allocation!
//
// Note: As of now this requires a bit of RAM to get going - but nothing a crafty person can't replace..
//

#include "BitStream.hpp"
#include <map>
#include <utility>
#include <queue>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "FastHuffman.h"
#include <string.h>

// Initialize the huffman encoding
FastHuffman::FastHuffman() : nPQItems(0),nTreeNodes(0) {
    Reset();
}

void FastHuffman::Reset() {
    ResetPQ();
    ResetTree();
    ResetHistogram();
}
void FastHuffman::ResetPQ() {
    nPQItems = 0;
    memset(pqItems,0, sizeof(PQItem) * FASTHUFF_VALUE_RANGE);
}
void FastHuffman::ResetTree() {
    nTreeNodes = 0;
    memset(nodes,-1, sizeof(TreeNode) * FASTHUFF_VALUE_RANGE * 2);
}
void FastHuffman::ResetHistogram() {
    memset(histogram,0, sizeof(T_FREQ) * FASTHUFF_VALUE_RANGE);
}

// Calculate the histogram/frequency table for the data
void FastHuffman::CalcHistogram(const uint8_t *data, size_t szData) {
    for(size_t i=0;i<szData;i++) {
        ++histogram[data[i]];
    }
}


// Dump the histogram
void FastHuffman::PrintHistogram() {
    printf("FastHuffMan::Histogram\n");
    for(int i=0;i<FASTHUFF_VALUE_RANGE-1;i++) {
        if (histogram[i] == 0) continue;
        char ch = '.';
        if ((i>31) && (i<128)) ch = i;
        printf("%.2x (%c) : %d\n", i, ch, histogram[i]);
    }
}

// Dump the queue
void FastHuffman::PrintPQueue() {
    printf("FastHuffMan::PQueue\n");
    for(int i=0; i < nPQItems; i++) {
        char ch = '.';
        if ((pqItems[i].value > 31) && (pqItems[i].value < 128)) ch = pqItems[i].value;
        printf("%.2x (%c) : %d\n", pqItems[i].value, ch, pqItems[i].freq);
    }
}

// Initialize a single node in the tree
FastHuffman::TreeNode *FastHuffman::InitNode(TreeNode *node, T_VALUE value, T_FREQ freq) {
//    node->value = value;
    node->idxParent = -1;   // -1 is our 'nullptr' indicator
    node->idxLeft = -1;
    node->idxRight = -1;
    return node;
}
bool FastHuffman::IsNodeValid(TreeNode *node) {
    if ((node->idxParent != -1) ||
            (node->idxLeft != -1) ||
            (node->idxRight != -1)) {
        return true;
    }
    return false;
}

// compare freq values - used when sorting the priority queue
static int fastHuffCmpPQItem(const void *p1, const void *p2) {
    auto *a = reinterpret_cast<const FastHuffman::PQItem *>(p1);
    auto *b = reinterpret_cast<const FastHuffman::PQItem *>(p2);
    return (a->freq>b->freq);
}

// This is actually very stupid, we re-sort the whole array after pushing a single value..
// Should be replaced with some kind of custom insert-sort
void FastHuffman::PushPQItem(T_VALUE value, T_FREQ freq) {
    pqItems[nPQItems] = PQItem{value, freq};
    nPQItems++;
    qsort(pqItems, nPQItems, sizeof(PQItem), fastHuffCmpPQItem);

}

// Pop an item from the priority queue
FastHuffman::PQItem FastHuffman::PopPQItem() {
    auto item = pqItems[0];
    for(int i=1; i < nPQItems; i++) {
        pqItems[i - 1] = pqItems[i];
    }
    nPQItems--;
    return item;
}

// Checks if there are pqItems left in the priority queue
bool FastHuffman::PQIsEmpty() {
    return (nPQItems == 0);
}

//
// Builds the huffman tree - this is an in-place tree using index
// NOTE: Once the tree is built we no longer need the priority queue
//       also, the tree actually doesn't need to store the leave value - it's given implicitly by the index
//
//
// TODO: rearrange this, once done we don't need the actual tree - instead the path through the tree
//       can be seen as a code - this is the code we want to save..
//       once, done we should reorganize the codes according to: https://en.wikipedia.org/wiki/Canonical_Huffman_code
//       this way only the code lengths matter...
//
void FastHuffman::BuildTree() {
    nTreeNodes = 0;
    // Setup base tree - these are all the leaves...
    // Plus initialize the priority queue (PQ) - which is basically just a sorted version of the histogram
    for(int i=0;i<FASTHUFF_VALUE_RANGE-1;i++) {
        if (histogram[i] == 0) continue;
        InitNode(&nodes[i], i, histogram[i]);
        PushPQItem(i, histogram[i]);
        nTreeNodes++;
    }

    PrintPQueue();
    // Iterate until we don't have anything
    int iteration = 1;
    while(!PQIsEmpty()) {
        printf("====================\n");
        printf("IT: %d\n", iteration);
        iteration++;
        auto itemA = PopPQItem();
        if (PQIsEmpty()) {
            // Done...
            // root is: iteamA.value
            break;
        }
        auto itemB = PopPQItem();
        // since the value is the index we can do this...
        TreeNode *leafA = &nodes[itemA.value];
        TreeNode *leafB = &nodes[itemB.value];

        uint32_t parentFreq = itemA.freq + itemB.freq;

        // Create the intermediate parent node, and push it to the queue...
        int idxInterMediateNode = nTreeNodes;
        TreeNode *parent = InitNode(&nodes[idxInterMediateNode], idxInterMediateNode, parentFreq);
        PushPQItem(idxInterMediateNode, parentFreq);

        // Connect the nodes - i.e. assign pointers
        leafA->idxParent = idxInterMediateNode;
        leafB->idxParent = idxInterMediateNode;
        parent->idxLeft = itemA.value;
        parent->idxRight = itemB.value;
        nTreeNodes++;
        PrintPQueue();
    }
    printf("nTreeNodes: %d\n", nTreeNodes);
    // All Leaf nodes are not initialized and linked properly with the intermediate nodes
    // Compression will now walk the tree bottom up and decompression will read bits and traverse until a leaf
    // has been hit...
}

// Generates a easy to debug bit-path for a certain value
std::vector<uint8_t> FastHuffman::PathForValue(uint8_t value) {
    std::vector<uint8_t> path;
    while(nodes[value].idxParent != -1) {
        auto idxParent = nodes[value].idxParent;
        if (idxParent != -1) {
            if (nodes[idxParent].idxLeft == value) {
                path.push_back(0);
            } else {
                path.push_back(1);
            }
        }
        value = idxParent;
    }
    // Reverse the path - this is needed to traverse the tree during decompression...
    // NOTE: Only in C++20
    std::reverse(path.begin(), path.end());
    return path;
}

// Prints the path
void FastHuffman::PrintPath(uint8_t value, std::vector<uint8_t> &path) {
    printf("Path for: %.2x (%c)\n", value, value);

    for(auto v : path) {
        printf("%d",v);
    }
    printf("\n");
}

// Compress a data set using the existing tree...
void FastHuffman::CompressWithTree(const uint8_t *data, size_t szData) {
    for(int i=0;i<szData;i++) {
        auto path = PathForValue(data[i]);
        PrintPath(data[i], path);
    }
}
void FastHuffman::PrintTree() {
    // Baseline algorithm
    // 1) Compute bit-length for each value
    // 2) store in array
    // 3) sort based on length ascending order (shortest first)
    // 4) impl. code print according to: https://en.wikipedia.org/wiki/Canonical_Huffman_code (see bottom of page)

    // We could reuse the Priority queue - once the table has been computed it is no longer needed...
    ResetPQ();
    for(int i=0;i<FASTHUFF_VALUE_RANGE;i++) {
        //if (!IsNodeValid(&nodes[i])) continue;
        if (histogram[i] == 0) continue;
        auto path = PathForValue(i);
        PushPQItem(i,path.size());
    }
    printf("Encoded symbol lengths\n");
    for(int i=0;i<nPQItems;i++) {
        // reusing priority queue - freq is used as bitlength..
        printf("%.2x (%c), %d\n", pqItems[i].value, pqItems[i].value, pqItems[i].freq-1);
    }
    //
    // Store like:
    //   <num sym for length>
    //   <symbols>
    //
    //  (1,1,2) (C,A,B,D)
    //   Length:  Num:   Symbols:
    //      0      1        C       -> will put a leaf directly under the root left
    //      1      1        A
    //      2      2       B,D
    //
    // Restore tree
    // 1) Insert left most if free
    // 2) Otherwise, traverse right
    // Seen from the tree's root point of view..
    // D: 101 - R-L-R
    // B: 100 - R-L-L
    // A: 11  - R-R
    // C: 0   - L
    //
    //       x
    //      /\
    //     C /\
    //      /\ A
    //     B  D
    //


}