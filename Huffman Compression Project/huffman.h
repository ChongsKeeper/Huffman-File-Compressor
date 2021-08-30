#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <memory>

/*
Classes and functions for Huffman compression.

This file intentionally leaves out any method of reading or writng files to maintain
flexibility in implementation.
*/



// There were a few ways I found to store the path to each leaf in the tree, but vector<bool> seemed the most effective.
// boolean vectors have inconsistent behavior to other vectors, but for this use case they work well and use very little memory. 
typedef std::vector<bool> bitVector;

namespace huffman
{
	// Constant used in branch nodes as a non-character.
	constexpr unsigned int NOT_A_CHAR = 256;

	struct Node
	{
		std::shared_ptr<Node> left;
		std::shared_ptr<Node> right;

		int freq;
		int character;

		// Construct a branch Node.
		Node(std::shared_ptr<Node> _left, std::shared_ptr<Node> _right);

		// Construct a leaf Node.
		Node(int _character, int _freq);
	};
	
	// Anonymous namespace to hide these two functions from the rest of the program.
	namespace
	{
		// Builds a Huffman tree from a frequency table. Needed for both the Encoder and the Decoder.
		std::shared_ptr<Node> buildTree(std::map<uint8_t, int> freqTable);
		// Pops the smallest node from the vector.
		std::shared_ptr<Node> getSmallest(std::vector<std::shared_ptr<Node>>& nodeVec);

		void progressBar(int &curPerc, int &prevPerc, int bytes, int fileLen);
	}

	// Handles Huffman encoding
	class Encoder
	{
	public:
		// fileLen is used for progress output.
		Encoder(unsigned int fileLen);

		// Can be called in mutliple times. Adds every character in the input string to the frequency table.
		void buildFreqTable(std::string input);

		// Uses the frequency table to build the Huffman Tree.
		void buildEncodingTree();

		// Overwrites the input string. Used to encode in chunks and writes leftover bits to the buffer.
		void encode(std::string& data);

		// Returns the remaing bits.
		uint8_t getBuffer();

		// getter method for m_freqTable
		std::map<uint8_t, uint32_t> freqTable();

		// getter method for m_compressedSize
		int compressedSize();

	private:
		// Table of the frequency with which each byte in the input occurs.
		std::map<uint8_t, uint32_t> m_freqTable;

		// The path to each leaf in the tree.
		std::map<uint8_t, bitVector> m_binMap;

		// The root Node of the Huffman tree
		std::shared_ptr<Node> m_huffmanTree;

		// Buffer for storing leftover bits after each encode() call
		uint8_t m_buffer;
		int m_curBit;
		int m_bytesProcessed;
		int m_fileLen;
		int m_compressedSize;

		// Used to output progress
		int m_percentCompressed;
		int m_prevPercent;

		// Called by buildEncodingTree() as there is no situation that doesn't require this to be called following that step.
		void buildBinMap(std::shared_ptr<Node>& curNode, bitVector& path);
	};

	// Handles Huffman decoding
	class Decoder
	{
	public:
		Decoder(std::map<uint8_t, uint32_t> freqTable, int fileLen);

		// Overwrites input string. Used to decode in chunks.
		void decode(std::string& input);

		// Returns true when the decoder has processed bytes equal to the file length.
		bool done();

	private:
		// The root node of the huffman tree
		std::shared_ptr<huffman::Node> m_hTree; 

		// Stores the current node during and between decode() calls
		std::shared_ptr<huffman::Node> m_curNode;

		int m_curByte;
		int m_fileLen;

		// Used to print the percentage of the file decompressed.
		int m_percentDecoded;
		int m_prevPercent;
	};
}



