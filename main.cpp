//
// Huffman compression - written for clarity, using C++17 or better..
// Note: the file-save routine does NOT save the huffman tree - which is needed to decompress...
//
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <queue>
#include <utility>
#include <map>
#include <queue>
#include "BitStream.hpp"
#include "FastHuffman.h"

static size_t fsize(const char *filename) {
    struct stat buf;
    int err = stat(filename, &buf);
    if (err) {
        return 0;
    }
    return buf.st_size;
}

static uint8_t *fload(const char *filename, size_t *outSize) {
    size_t szFile = fsize(filename);
    if (!szFile) {
        return nullptr;
    }
    FILE *f = fopen(filename, "r");
    if (!f) {
        return nullptr;
    }
    auto *buffer = (uint8_t *)malloc(szFile);
    auto nRead = fread(buffer, 1, szFile, f);
    if (nRead != szFile) {
        free(buffer);
        return nullptr;
    }
    fclose(f);
    *outSize = szFile;
    return buffer;
}


struct Node {
    struct Node *parent;
    struct Node *left, *right;
    int val;
    uint32_t freq;
};

// Priority queue pair, first = frequency, second = node
typedef std::pair<uint32_t, struct Node *> PQPair;
// histogram hash-table, key = byte in uncompressed data, value = frequency
typedef std::map<uint8_t, uint32_t> HISTOGRAM;
// hash table to speed up compression... this could be a flat array [0..255] initially set all to nullptr...
typedef std::map<uint8_t, struct Node *> NodeLeafs;

// Using priority queue for the nodes, but basically this is an array with insertion sort
// based on frequency...
typedef std::priority_queue<PQPair, std::vector<PQPair>, std::greater<>> PQ;

static void calcHistogram(HISTOGRAM &histogram, const uint8_t *data, size_t szData) {
    // Insert all zeros
    for(int i=0;i<255;i++) {
        histogram[i] = 0;
    }
    // Now calculate frequency of each byte
    for(size_t i=0;i<szData;i++) {
        uint8_t byte = data[i];
        histogram[byte] = histogram[byte] + 1;
    }
}
static void printHistogram(HISTOGRAM &histogram) {
    for(int i=0;i<255;i++) {
        if (histogram[i] == 0) continue;
        char ch = '.';
        if ((i > 31) && (i<128)) ch = i;
        printf("%.2x (%c) : %d\n", i,ch,histogram[i]);
    }
}

static Node *createNode(uint8_t val, uint32_t freq) {
    auto node = new Node();
    node->left = nullptr;
    node->right = nullptr;
    node->parent = nullptr;
    node->val = val;
    node->freq = freq;
    return node;
}

static void printQueue(PQ data) {
    while(!data.empty()) {
        auto node = data.top().second;
        uint8_t ch = node->val > 31 ? node->val : '.';
        printf("%.3d:%c (%d)\n", ch, ch, node->freq);
        data.pop();
    }
}

//
// This builds a verbose pointer based tree from the histogram/frequency table
// tree is built using priority queues..
//
static NodeLeafs buildHuffmanTree(HISTOGRAM &histogram) {
    PQ nodes;
    NodeLeafs leafs;
    for(auto itr : histogram) {
        // Only create nodes for bytes with present in the uncompressed data (freq > 0)
        if (itr.second > 0) {
            auto node = createNode(itr.first, itr.second);
            // Put the nodes into the priority queue..
            nodes.push(PQPair(node->freq, node));
            // Also, create a hash-map of the nodes..
            leafs[node->val] = node;
        }
    }
    printf("Full Queue: value (freq)\n");
    printQueue(nodes);

    // This is the gist of it - once the priority queue has been created we can build the tree
    int iteration = 1;
    struct Node *parent;
    while(!nodes.empty()) {
        printf("====================\n");
        printf("IT: %d\n", iteration);
        iteration++;
        auto leafA = nodes.top();
        nodes.pop();
        if (nodes.empty()) {
            parent = leafA.second;
            break;
        }
        auto leafB = nodes.top();
        nodes.pop();
        parent = createNode(-1, leafA.second->freq + leafB.second->freq);
        // Connect the nodes - i.e. assign pointers
        leafA.second->parent = parent;
        leafB.second->parent = parent;
        parent->left = leafA.second;
        parent->right = leafB.second;
        nodes.push(PQPair(parent->freq, parent));
        printQueue(nodes);
    }
    return leafs;
}

static std::vector<uint8_t> huffmanPathForValue(NodeLeafs &nodeLeafs, uint8_t value) {
    auto node = nodeLeafs[value];
    std::vector<uint8_t> path;
    //printf("startNode for %.2x : %.2x (%c)\n", value, node->val, node->val);
    while(node != nullptr) {
        if (node->parent != nullptr) {
            if (node->parent->left == node) {
                path.push_back(0);
            } else {
                path.push_back(1);
            }
        }
        node = node->parent;
    }
    // Reverse the path - this is needed to traverse the tree during decompression...
    std::reverse(path.begin(), path.end());
    return path;
}
static void printPath(uint8_t value, std::vector<uint8_t> &path) {

    printf("Path for: %.2x (%c)\n", value, value);
    for(auto v : path) {
        printf("%c",v?'1':'0');
    }
    printf("\n");
}


static void compressWithHuffmanTree(NodeLeafs &nodeLeafs, const uint8_t *data, size_t szData) {
    BitStream bitStream;
    for(int i=0;i<szData;i++) {
        auto path = huffmanPathForValue(nodeLeafs, data[i]);
        printPath(data[i], path);
        bitStream.Write(path);
    }
    if (bitStream.FlushToFile("Compressed.bin")) {
        printf("OK, file saved\n");
    }
}




int main() {

    size_t sz;
    printf("Loading file\n");
    // test.txt: 'BCAADDDCCACACAC'
    auto data = fload("test.txt", &sz);
    if (data == nullptr) {
        printf("Unable to load JSON\n");
        return -1;
    }
    HISTOGRAM histogram;
    printf("Computing histogram\n");
    calcHistogram(histogram, data, sz);
    printHistogram(histogram);
    printf("Building huffman tree\n");
    auto tree = buildHuffmanTree(histogram);
    // Use the huffman tree to compress the following: 'ABCD'
    // Should output:  A  B  C  D
    //                11 100 0 101 => 1110 0010 | 1xxx xxxx
    // => 0xe2 0x80
    //
    // There will by 7 stray bits - ergo, we need 2 bytes...
    //
    compressWithHuffmanTree(tree, reinterpret_cast<const uint8_t *>("ABCD"), 4);
    printf("FAST HUFF\n");
    FastHuffman fastHuffman{};
    fastHuffman.CalcHistogram(data, sz);
    fastHuffman.PrintHistogram();
    fastHuffman.BuildTree();
    fastHuffman.CompressWithTree(reinterpret_cast<const uint8_t *>("ABCD"), 4);
}
