//
// Created by Fredrik Kling on 18/02/2022.
//

#ifndef ASCIICOMP_BITSTREAM_HPP
#define ASCIICOMP_BITSTREAM_HPP

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

//
// Stupid lousy bitstream
//
class BitStream {
public:
    BitStream() = default;
    ~BitStream() = default;

    void Write(uint8_t v) {
        bits.push_back(v?true:false);
    }

    void Write(const std::vector<uint8_t> &data) {
        for(auto v : data) {
            bits.push_back(v?true:false);
        }
    }

    void Write(bool bit) {
        bits.push_back(bit);
    }

    bool FlushToFile(const char *filename) {
        FILE *f = fopen(filename, "w+");
        if (!f) {
            printf("Failed to open '%s'\n", filename);
            return false;
        }
        uint8_t current = 0;
        for(int i=0;i<bits.size();i++) {
            current |=  (bits[i]?1:0) << (7 - i & 7);
            if ((i & 7) == 7) {
                fwrite(&current, 1, 1, f);
                current = 0;
            }
        }
        // Write any stray...
        if ((bits.size() & 7) != 0) {
            fwrite(&current, 1, 1, f);
        }
        fclose(f);
        return true;
    }

    size_t ByteSize() {
        auto sz = bits.size()>>3;
        if ((bits.size() & 7) != 0) {
            sz += 1;
        }
        return sz;
    }

private:
    std::vector<bool> bits;
};

#endif //ASCIICOMP_BITSTREAM_HPP
