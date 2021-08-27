#include "huffman.h"

namespace huffman
{
	Node::Node(std::shared_ptr<Node> _left, std::shared_ptr<Node> _right)
		: left(_left)
		, right(_right)
		, character(NOT_A_CHAR)
		, freq(left->freq + right->freq)
	{ }
	
	Node::Node(int _character, int _freq)
		: left(nullptr)
		, right(nullptr)
		, character(_character)
		, freq(_freq)
	{ }

	namespace
	{
		std::shared_ptr<Node> buildTree(std::map<uint8_t, int> freqTable)
		{
			std::vector<std::shared_ptr<Node>> nodes;

			// Populate the nodes vector from the frequency table
			for (auto& node : freqTable)
			{
				nodes.push_back(std::make_shared<Node>(node.first, node.second));
			}

			// Keep making a new Node as the parent of the two smallest nodes in the tree until there is only one Node left in the vector.
			while (nodes.size() > 1)
			{
				nodes.push_back(std::make_shared<Node>(getSmallest(nodes), getSmallest(nodes)));
			}

			return nodes[0];
		}

		std::shared_ptr<Node> getSmallest(std::vector<std::shared_ptr<Node>>& nodeVec)
		{
			// Find the smallest node in the vector.
			size_t smallest = 0;
			for (size_t i = 0; i < nodeVec.size(); i++)
			{
				if (nodeVec[i]->freq < nodeVec[smallest]->freq)
					smallest = i;
			}

			// Make a new pointer for the smallest node and remove it from the vector
			std::shared_ptr<Node> smallNode = nodeVec[smallest];
			nodeVec.erase(nodeVec.begin() + smallest);

			return smallNode;
		}
	}

	Encoder::Encoder(unsigned int fileLen)
		: m_huffmanTree(nullptr)
		, m_buffer(0)
		, m_curBit(0)
		, m_compressedSize(0)
		, m_fileLen(fileLen)
		, m_bytesProcessed(0)
		, m_percentCompressed(0)
		, m_prevPercent(0)
	{ }

	void Encoder::buildFreqTable(std::string input)
	{
		for (uint8_t byte : input)
		{
			m_freqTable[byte] += 1;
		}
	}

	void Encoder::buildEncodingTree()
	{
		// Call the shared buildTree function
		m_huffmanTree = buildTree(m_freqTable);

		// Create the binary map from the tree. 
		bitVector path;
		buildBinMap(m_huffmanTree, path);
	}

	void Encoder::buildBinMap(std::shared_ptr<Node>& curNode, bitVector& path)
	{
		// Path is passed by reference through each call. Each time it reaches a leaf it saves that permutation of the
		// path to the binary map. As the calls resolve it swaps or removes the last bit of the Path.

		if (curNode->character != NOT_A_CHAR)
		{
			m_binMap[curNode->character] = path;
			return;
		}

		// 0 represents the left path
		path.push_back(0);
		buildBinMap(curNode->left, path);

		// Swap the 0 to a 1
		path.back() = 1;
		buildBinMap(curNode->right, path);

		// Remove the 1 to return to the orignal value Path entered this function call with
		path.pop_back();
	}

	void Encoder::encode(std::string& data)
	{
		std::string encodedData;
		for (const auto& character : data)
		{
			m_bytesProcessed++;

			// Terminal output of compression percentage.
			m_percentCompressed = round(float(m_bytesProcessed) / float(m_fileLen) * 100);
			if (m_percentCompressed != m_prevPercent)
			{
				std::cout << "compressing... " << m_percentCompressed << "%\t\t" << m_bytesProcessed / 1024 << "/" << m_fileLen / 1024 << " KB\n";
			}
			m_prevPercent = m_percentCompressed;

			// Write the binary code that corresponds to the current character. 
			for (auto bit : m_binMap[character])
			{
				m_curBit++;
				m_buffer <<= 1;
				m_buffer |= bit;
				if (m_curBit == 8)
				{
					encodedData += m_buffer;
					m_curBit = 0;
					m_compressedSize += 1;
				}
			}
		}
		data = encodedData;
	}

	uint8_t Encoder::getBuffer()
	{
		// Only increments compressed size if the buffer contained any data.
		if (m_buffer != 0) m_compressedSize += 1;

		auto buffer = m_buffer << 8 - m_curBit;
		m_buffer = 0;
		return buffer;
	}

	std::map<uint8_t, int> Encoder::freqTable()
	{
		return m_freqTable;
	}

	int Encoder::compressedSize()
	{
		return m_compressedSize;
	}


	Decoder::Decoder(std::map<uint8_t, int> freqTable, int fileLen)
		: m_hTree(buildTree(freqTable))
		, m_curNode(m_hTree)
		, m_curByte(0)
		, m_fileLen(fileLen)
		, m_percentDecoded(0)
		, m_prevPercent(0)
	{ }

	void Decoder::decode(std::string& data)
	{
		std::string decodedData = "";
		int bit = 0;

		for (const auto& byte : data)
		{
			for (int i = 7; i >= 0; i--)
			{
				// Check if the current Node is a leaf.
				if (m_curNode->character != NOT_A_CHAR)
				{
					m_curByte++;
					// Don't add a character if it's past the end of the file.
					if (m_curByte <= m_fileLen)
					{
						// Add the leaf to the decoded string.
						decodedData += m_curNode->character;

						// Print the percentage to the terminal.
						m_percentDecoded = round((float)m_curByte / (float)m_fileLen * 100);
						if (m_percentDecoded != m_prevPercent)
						{
							std::cout << "decompressing... " << m_percentDecoded << "%\t\t" << m_curByte / 1024 << "/" << m_fileLen / 1024 << " KB\n";
						}
						m_prevPercent = m_percentDecoded;
					}
					// Reset the current node back to the root.
					m_curNode = m_hTree;
				}
				// Traverse the tree.
				bit = (byte >> i) & 1U;
				// Go left
				if (bit == 0)
				{
					m_curNode = m_curNode->left;
				}
				// Go Right
				else if (bit == 1)
				{
					m_curNode = m_curNode->right;
				}
			}
		}
		data = decodedData;
	}

	bool Decoder::done()
	{
		return m_curByte >= m_fileLen;
	}
}