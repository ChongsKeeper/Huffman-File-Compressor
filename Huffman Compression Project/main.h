#pragma once
#include <iostream>
#include <fstream>
#include <map>
#include <stdio.h>
#include "huffman.h"
#include "ThirdParty/CLI11.hpp"
#include "ThirdParty/md5.h"



struct FileVersion
{
    uint8_t major;
    uint8_t minor;
};

struct Header
{
    FileVersion fileVersion;
    std::string hash;
    int fileSize;
    int compressedSize;
    std::string filename;
    std::map<uint8_t, uint32_t> freqTable;

    Header()
        : fileVersion{ 0, 0 }
        , hash("")
        , fileSize(0)
        , compressedSize(0)
        , filename("")
        , freqTable{ }
    { }
};

// The largest number of bytes that can be sent to the encoder.
constexpr unsigned int MAX_BUFFER = 8192;

// The current version of the compression program. Useful for confirming which version 
// of this program wrote the compressed file so it can be handled correctly.
constexpr FileVersion curFileVersion = { 1,1 };

// A unique byte set that is placed at the start of a compressed file. This is the first thing checked when decompressing a file.
const std::string uniqueSig = "ANHC";





// The meat of the program. These functions handle all the file reading and writing.

void compress(std::string filename, std::string path);

// This function feeds the encoder chunks of data so that it can populate its frequency table and produce a huffman tree.
// It also feeds the MD5 hash generator. This isn't exactly related to the function as a whole, but it means the file only needs to be read twice.
void createPrefix(std::ifstream& input, unsigned int fileLen, huffman::Encoder& encoder, MD5& md5);

// Takes all of the necessary data for decompression and writes it to the output. Returns the offset for compressed size.
int writeHeader(std::ofstream& output, unsigned int fileLen, std::string filename, std::map<uint8_t, uint32_t> freqTable, MD5& md5);

// The actual compression of the file.
void encodeFile(std::ifstream& input, unsigned int fileLen, std::ofstream& output, huffman::Encoder& encoder);

// Includes constant checks for validity in the input file. If it makes it all the way through,
// a final check against the MD5 hash will delete the newly written file if the hash doesn't match.
void decompress(std::string filename, std::string path, bool overwriteFlag, bool keepFlag);

// Returns a header object containing all of the header data.
Header readHeader(std::ifstream& input);

bool checkSig(std::ifstream& filename);

void listContents(std::string filename);



// Utility functions for dealing with file names and paths.

// Replaces an existing extension or appends .huf to the end of a filename to distinguish compressed files from their originals.
std::string replaceExtension(std::string filename);

// Removes the path to a file from a string
std::string removePath(const std::string& filename);

// Make sure the path ends with a "/".
void pathEndSlash(std::string& path);




/*
Endianess was an interesting issue to think about. Through research I found many ways of determining the order of bytes on local hardware and performing swaps
to place it in the correct order for output, but these methods seemed overly complicated and difficult to implement in an elegant fashion.
I settled on the method of handling endianess found in this article: https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html

Essentially, local byte order doesn't matter. Bit shift operators will perform the same operation on an integer no matter what the local endianess is. Because of this,
the only thing that matters is what is being written to or read from.

These two functions use an array of uint8_ts to control the order the file is interacted with, converting
local integers to big endian and vice versa, regardless of local architecture.
*/

bool writeInt(std::ofstream& output, uint32_t num);
uint32_t readInt(std::ifstream& input);