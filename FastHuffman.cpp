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

// Initialize the huffman encoding
FastHuffman::FastHuffman() : nItems(0) {
    memset(histogram,0, sizeof(T_FREQ) * FASTHUFF_VALUE_RANGE);
    memset(nodes,0, sizeof(TreeNode) * FASTHUFF_VALUE_RANGE * 2);
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
    for(int i=0;i<nItems;i++) {
        char ch = '.';
        if ((items[i].value>31) && (items[i].value<128)) ch = items[i].value;
        printf("%.2x (%c) : %d\n", items[i].value,ch,items[i].freq);
    }
}

// Initialize a single node in the tree
FastHuffman::TreeNode *FastHuffman::InitNode(TreeNode *node, T_VALUE value, T_FREQ freq) {
    node->value = value;
    node->freq = freq;
    node->idxParent = -1;   // -1 is our 'nullptr' indicator
    node->idxLeft = -1;
    node->idxRight = -1;
    return node;
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
    items[nItems] = PQItem{value, freq};
    nItems++;
    qsort(items, nItems, sizeof(PQItem),fastHuffCmpPQItem);

}

// Pop an item from the priority queue
FastHuffman::PQItem FastHuffman::PopPQItem() {
    auto item = items[0];
    for(int i=1;i<nItems;i++) {
        items[i-1] = items[i];
    }
    nItems--;
    return item;
}

// Checks if there are items left in the priority queue
bool FastHuffman::PQIsEmpty() {
    return (nItems == 0);
}

//
// Builds the huffman tree - this is an in-place tree using index
//
void FastHuffman::BuildTree() {
    int nTreeNodes = 0;
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
        TreeNode *parent = InitNode(&nodes[nTreeNodes], nTreeNodes, parentFreq);
        PushPQItem(nTreeNodes, parentFreq);

        // Connect the nodes - i.e. assign pointers
        leafA->idxParent = nTreeNodes;
        leafB->idxParent = nTreeNodes;
        parent->idxLeft = leafA->value;
        parent->idxRight = leafB->value;
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