#include "main.h"

/*
This program is a command line based Huffman compressor. It was created as a portfolio piece for the SMU Guildhall Fall 2022 application.
It uses two external resources: md5.h (and its associated .cpp file) by Stephan Brumme https://create.stephan-brumme.com/ and CLI11, a command line parser https://github.com/CLIUtils/CLI11

Commands:
            Filename
-d			Decompress
-o			Overwrite
-p, --path	Path for output
-k          Debug tool. Prevents the program from deleting unencoded files when the hash is incorrect
-l          List the contents of a .huf file.

*/

int main(int argc, char** argv)
{
	// CLI app object that parses the command line.
	CLI::App app{ "Huffman Compression algorithm" };

	// Filename
	std::string filename = "default";
	app.add_option("filename", filename, "The name of the file to be compressed/decompressed")->check(CLI::ExistingFile);

	//std::string outFilename = "";
	//app.add_option("outFilename", outFilename, "The output file name");
	
	// Path: -p, --path        Specify output file path
	std::string path = "";
	app.add_option("-p, --path", path, "Optional. Specifies path that new file will be written to")->check(CLI::ExistingPath);

	// Flags
	// Decompress: -d
	bool decompressFlag = false;
	app.add_flag("-d", decompressFlag, "Include to decompress");

	// Overwrite: -o
	bool overwriteFlag = false;
	app.add_flag("-o", overwriteFlag, "Include to overwrite existing file");

	// Keep file if faulty. Used for debug purposes
	bool keepFlag = false;
	app.add_flag("-k", keepFlag, "Include to prevent bad files from being deleted on hash checking");

	// List: -l                Lists contents of .huf file header.
	bool listFlag = false;
	app.add_flag("-l, --list", listFlag, "Include to list the contents of decompressed file");

	// Macro that tells CLI to parse the command line.
	CLI11_PARSE(app, argc, argv);

	if (listFlag)
	{
		listContents(filename);
	}
	else if (decompressFlag)
	{
		decompress(filename, path, overwriteFlag, keepFlag);
	}
	else
	{
		compress(filename, path);
	}
	
	return 0;
}


std::string replaceExtension(std::string filename)
{
	// If there is a "." in the last four characters, it replaces those characters with ".huf", otherwise appends ".huf".
	auto extPos = filename.rfind('.');
	int extLen = 4;
	if (filename.size() - extLen <= extPos)
	{
		filename.replace(extPos, extLen, ".huf");
	}
	else
	{
		filename += ".huf";
	}
	return filename;
}

std::string removePath(const std::string& filename)
{
	// Find the last "/" and return the sub string following it. Return the full string if there wasn't a "/".
	size_t pathDiv = filename.find_last_of('/');
	if (pathDiv != std::string::npos)
	{
		return filename.substr(pathDiv + 1);
	}
	return filename;
}


void compress(std::string filename, std::string path)
{
	std::ifstream input(filename, std::ios::binary);
	if (!input.good())
	{
		std::cerr << "ERROR: File \" " << filename << " \"was not able to be opened.\n";
		return;
	}

	input.seekg(0, input.end);
	unsigned int fileLen = input.tellg();
	input.seekg(0, input.beg);

	huffman::Encoder encoder(fileLen);
	MD5 md5;

	// Create the frequency table and MD5 hash
	createPrefix(input, fileLen, encoder, md5);
	encoder.buildEncodingTree();

	// Remove the path from the filename, if it has one, replace the extension on the output name and add the path for writing.
	filename = removePath(filename);
	std::string outFilename = path + replaceExtension(filename);
	std::ofstream output(outFilename, std::ios::binary);

	// Make sure output file created.
	if (!output.good())
	{
		std::cerr << "ERROR: output file failed to create.\n";
		return;
	}

	// Write the header and return the offset of the compressed size bytes.
	int cmprSizeOffset = writeHeader(output, fileLen, filename, encoder.freqTable(), md5);

	// Reset the head of the input stream and encode the whole thing.
	input.seekg(0, input.beg);
	encodeFile(input, fileLen, output, encoder);

	// Write the compressed size to the saved location.
	auto compressedSize = encoder.compressedSize();
	output.seekp(cmprSizeOffset, output.beg);
	writeInt(output, compressedSize);
	std::cout << "\n";
}

void createPrefix(std::ifstream& input, unsigned int fileLen, huffman::Encoder& encoder, MD5& md5)
{
	unsigned int curByte = 0;
	std::string buffer;

	while (curByte < fileLen)
	{
		// Reads the file in chunks
		buffer.resize(fileLen - curByte > MAX_BUFFER ? MAX_BUFFER : fileLen - curByte);
		input.read(&buffer[0], buffer.size());
		curByte += buffer.size();

		// MD5 hashing
		md5.add(buffer.data(), buffer.size());

		encoder.buildFreqTable(buffer);
	}
}

int writeHeader(std::ofstream& output, unsigned int fileLen, std::string filename, std::map<uint8_t, int> freqTable, MD5& md5)
{
	/*
	Header format:
	offset	bytes	description
	0		4		uniqueSig
	4		2		version
	6		32		MD5 hash
	38		4		original file size
	42		4		compressed file size
	46		1		filename length (n)
	47		n		filename
	47+n	1		freqTable size (f)
	48+n	(1+4)*f freqTable
	*/

	// Unique identifer and version number to prevent running the code on incorrectly formatted files when decompressing.
	output.write(uniqueSig.data(), uniqueSig.size());
	output.put(curFileVersion.major);
	output.put(curFileVersion.minor);

	// MD5 hashing to verify file integrity.
	output.write(md5.getHash().data(), md5.getHash().size());

	// Write the uncompressed file size.
	writeInt(output, fileLen);

	// Save the location of the compressed size bytes and skip them.
	uint32_t cmprSizeOffset = output.tellp();
	output.seekp(sizeof(cmprSizeOffset), output.cur);

	// Write the original filename.
	uint8_t fnsize = filename.size();
	output.write(reinterpret_cast<char*>(&fnsize), sizeof(fnsize));
	output.write(filename.data(), filename.size());

	// Write the size of the frequency table and the table for decompressing.
	uint8_t tableSize = freqTable.size();
	output.write(reinterpret_cast<char*>(&tableSize), sizeof(tableSize));
	for (auto& leaf : freqTable)
	{
		output.put(leaf.first);
		writeInt(output, leaf.second);
	}

	// Return the compressed size byte location. This gets written after compression.
	return cmprSizeOffset;
}

void encodeFile(std::ifstream& input, unsigned int fileLen, std::ofstream& output, huffman::Encoder& encoder)
{
	unsigned int curByte = 0;
	std::string buffer;

	// Feeds MAX_BUFFER size chunks of the input file to the encoder until it's done.
	while (curByte < fileLen)
	{
		buffer.resize(fileLen - curByte > MAX_BUFFER ? MAX_BUFFER : fileLen - curByte);

		input.read(&buffer[0], buffer.size());
		curByte += buffer.size();

		// encode() overwrites the buffer. Write the encoded string to the output.
		encoder.encode(buffer);
		output.write(buffer.data(), buffer.size());
	}

	// Retrieve remaining bits from the buffer and write to the output.
	buffer = encoder.getBuffer();
	output.write(buffer.data(), buffer.size());
}


void decompress(std::string filename, std::string path, bool overwriteFlag, bool keepFlag)
{
	// create the File Stream and check that it is a valid file.
	std::ifstream input(filename, std::ios::binary);
	if (!input.good())
	{
		std::cerr << "ERROR: File \" " << filename << " \"was not able to be opened.\n";
		return;
	}

	// Check that the file has the correct signature. If it wasn't compressed by this program the signauture will be missing.
	if (!checkSig(input)) return;

	Header header = readHeader(input);

	// Check if the file version is correct
	if (header.fileVersion.major != curFileVersion.major || header.fileVersion.minor != curFileVersion.minor)
	{
		std::cerr << "ERROR: Invalid file version.\n";
		return;
	}

	// Check if the Frequency Table has at least one entry. The program crashes when it tries to build a huffman tree from an empty table.
	if (header.freqTable.empty())
	{
		std::cerr << "ERROR: Frequency Table was empty.";
		return;
	}

	// The name of the output file with the path to write to.
	std::string outputName = path + header.filename;

	//  Check if the file already exists to prevent overwriting.
	std::ifstream tempStream(outputName);
	if (tempStream.good())
	{
		if (!overwriteFlag)
		{
			std::cout << "File already exists. Add -o to command line to overwrite.\n";
			return;
		}
	}

	std::ofstream output(outputName, std::ios::binary);
	if (!output.good())
	{
		std::cerr << "Output file failed to create\n";
		return;
	}

	// MD5 hashing to verify the integrity of the file.
	MD5 md5;

	// Create a decoder object. It generates the Huffman tree from the frequency table. fileSize tells it when to stop.
	huffman::Decoder decoder(header.freqTable, header.fileSize);

	// Read the file in chunks and write it to the output file. Also generates the MD5 hash.
	std::string buffer;
	while (!decoder.done())
	{
		buffer.resize(MAX_BUFFER);
		input.read(&buffer[0], buffer.size());

		decoder.decode(buffer);
		
		md5.add(buffer.data(), buffer.size());

		output.write(buffer.data(), buffer.size());
	}

	// Confirm the hash matches and delete the file if it doesn't.
	if (header.hash != md5.getHash())
	{
		output.close();
		std::cerr << "Corruption ERROR: New hash does not match saved hash\n";
		std::cout << md5.getHash();
		
		if (keepFlag)
		{
			std::cout << "Keeping bad file.\n";
		}
		else if (std::remove(outputName.c_str()))
		{
			std::cout << outputName << " was deleted.\n";
		}
		else
		{
			std::cout << outputName << " could not be deleted.\n";
		}
	}
	else
	{
		std::cout << "\nFile decompressed successfully.\n";
	}
}

Header readHeader(std::ifstream& input)
{
	/*
	Header format:
	offset	bytes	description
	0		4		uniqueSig
	4		2		version
	6		32		MD5 hash
	38		4		original file size
	42		4		compressed file size
	46		1		filename length (n)
	47		n		filename
	47+n	1		freqTable size (f)
	48+n	f*5		freqTable

	The signature is read before this function is called. If the signature is not the expected characters the program is terminated.
	*/

	Header header;

	header.fileVersion.major = input.get();
	header.fileVersion.minor = input.get();

	header.hash.resize(32);
	input.read(&header.hash[0], header.hash.size());

	header.fileSize = readInt(input);
	header.compressedSize = readInt(input);
	
	uint8_t nameLen;
	input.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
	header.filename.resize(nameLen);
	input.read(&header.filename[0], header.filename.size());

	uint8_t freqTableSize = 0;
	input.read(reinterpret_cast<char*>(&freqTableSize), sizeof(freqTableSize));

	for (unsigned int i = 0; i < freqTableSize; i++)
	{
		char key;
		input.get(key);
		int value = readInt(input);

		header.freqTable[key] = value;
	}

	return header;
}

bool checkSig(std::ifstream& input)
{
	std::string signature;
	signature.resize(uniqueSig.size());
	input.read(reinterpret_cast<char*>(&signature[0]), signature.size());

	if (signature != uniqueSig)
	{
		std::cerr << "ERROR: Invalid file.\n";
		return false;
	}

	return true;
}

void listContents(std::string filename)
{
	std::ifstream input(filename);

	if (!checkSig(input)) return;

	Header header = readHeader(input);

	std::cout << "Huffman Compression version: " << static_cast<unsigned int>(header.fileVersion.major) << "." << static_cast<unsigned int>(header.fileVersion.minor) << "\n"
		      << "Original file name:          " << header.filename << "\n"
		      << "Original file size:          " << (float)header.fileSize / 1024 << " KB" << "\n"
		      << "Compressed file size:        " << (float)header.compressedSize / 1024 << " KB" << "\n"
		      << "MD5 hash:                    " << header.hash << "\n";
}

bool writeInt(std::ofstream &output, uint32_t num)
{
	// Write a 4 byte integer in Big Endian

	uint8_t data[4]{};

	// Bit shifts convert local Endianess to Big Endian
	data[0] = num >> 24;
	data[1] = num >> 16;
	data[2] = num >> 8;
	data[3] = num >> 0;

	for (int i = 0; i < 4; i++)
	{
		output.write(reinterpret_cast<char*>(&data[i]), sizeof(data[i]));
	}

	return true;
}

uint32_t readInt(std::ifstream& input)
{
	// Read a 4 byte integer in Big Endian

	uint8_t data[4]{};

	for (int i = 0; i < 4; i++)
	{
		input.read(reinterpret_cast<char*>(&data[i]), sizeof(data[i]));
	}

	// Bit shifts read a Big Endian byte order and convert it to local Endianess
	uint32_t num = (data[3] << 0)
		         | (data[2] << 8)
		         | (data[1] << 16)
		         | (data[0] << 24);

	return num;
}