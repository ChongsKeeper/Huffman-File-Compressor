#include "main.h"

int main(int argc, char** argv)
{
	CLI::App app{ "Huffman Compression algorithm" };

	std::string filename = "default";
	app.add_option("filename", filename, "The name of the file to be compressed/decompressed")->check(CLI::ExistingFile);
	
	std::string path = "";
	app.add_option("-p, --path", path, "Optional. Specifies path that new file will be written to")->check(CLI::ExistingPath);

	bool decompressFlag = false;
	app.add_flag("-d", decompressFlag, "Include to decompress");
	bool overwriteFlag = false;
	app.add_flag("-o", overwriteFlag, "Include to overwrite existing file");

	CLI11_PARSE(app, argc, argv);

	if (!decompressFlag)
	{
		compress(filename, path);
	}
	else
	{
		decompress(filename, path, overwriteFlag);
	}
	
	return 0;
}


std::string replaceExtension(std::string filename)
{
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

	createPrefix(input, fileLen, encoder, md5);
	encoder.buildEncodingTree();

	filename = removePath(filename);
	std::string outFilename = path + replaceExtension(filename);
	std::ofstream output(outFilename, std::ios::binary);

	if (!output.good())
	{
		std::cerr << "ERROR: output file failed to create.\n";
		return;
	}

	writeHeader(output, fileLen, filename, encoder.freqTable(), md5);

	input.seekg(0, input.beg);
	encodeFile(input, fileLen, output, encoder);

	auto compressedSize = encoder.compressedSize();
	output.seekp(42, output.beg); // 42 is the offset for compressedSize in the header.
	output.write(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));
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

void writeHeader(std::ofstream& output, unsigned int fileLen, std::string filename, std::map<uint8_t, int> freqTable, MD5& md5)
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
	*/

	// Unique identifer and version number to prevent running the code on incorrectly formatted files when decompressing.
	output.write(uniqueSig.data(), uniqueSig.size());
	output.put(curFileVersion.major);
	output.put(curFileVersion.minor);

	// MD5 hashing to verify file integrity.
	output.write(md5.getHash().data(), md5.getHash().size());

	// Write the uncompressed file size and skip the bytes that will store the compressed file size.
	output.write(reinterpret_cast<char*>(&fileLen), sizeof(fileLen));
	output.seekp(sizeof(fileLen), output.cur);

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
		output.write(reinterpret_cast<char*>(&leaf.second), sizeof(leaf.second));
	}
}

void encodeFile(std::ifstream& input, unsigned int fileLen, std::ofstream& output, huffman::Encoder& encoder)
{
	unsigned int curByte = 0;
	std::string buffer;

	while (curByte < fileLen)
	{
		buffer.resize(fileLen - curByte > MAX_BUFFER ? MAX_BUFFER : fileLen - curByte);

		input.read(&buffer[0], buffer.size());
		curByte += buffer.size();

		encoder.encode(buffer);
		output.write(buffer.data(), buffer.size());
	}

	buffer = encoder.getBuffer();
	output.write(buffer.data(), buffer.size());
}


void decompress(std::string filename, std::string path, bool overwriteFlag)
{
	// create the File Stream and check that it is a valid file.
	std::ifstream input(filename, std::ios::binary);
	if (!input.good())
	{
		std::cerr << "ERROR: File \" " << filename << " \"was not able to be opened.\n";
		return;
	}

	// Check that the file has the correct signature. If it wasn't compressed by this program the signauture will be missing.
	std::string signature;
	signature.resize(4);
	input.read(reinterpret_cast<char*>(&signature[0]), signature.size());

	if (signature != uniqueSig)
	{
		std::cerr << "ERROR: Invalid file.\n";
		return;
	}

	auto header = readHeader(input);

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

	//  Check if the file already exists to prevent overwriting.
	if (!overwriteFlag)
	{
		std::ifstream tempStream(header.filename);
		if (tempStream.good())
		{
			std::cout << "File already exists. Add -o to command line to overwrite.\n";
			return;
		}
		tempStream.close();
	}
	
	std::string outfile = path + header.filename;

	huffman::Decoder decoder(header.freqTable, header.fileSize);
	std::ofstream output(outfile, std::ios::binary);

	if (!output.good())
	{
		std::cerr << "ERROR: Unable to open output file.\n";
		return;
	}

	// MD5 hashing to verify the integrity of the file.
	MD5 md5;

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
		try
		{
			if (std::filesystem::remove(header.filename))
			{
				std::cout << header.filename << " was deleted.\n";
			}
			else
			{
				std::cout << header.filename << " could not be deleted.\n";
			}
		}
		catch (const std::filesystem::filesystem_error& err)
		{
			std::cerr << "filesystem error: " << err.what() << '\n';
		}
	}
	else
	{
		std::cout << "File decompressed successfully.";
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

	input.read(reinterpret_cast<char*>(&header.fileSize), 4);
	input.read(reinterpret_cast<char*>(&header.compressedSize), 4);
	
	uint8_t nameLen;
	input.read(reinterpret_cast<char*>(&nameLen), 1);
	header.filename.resize(nameLen);
	input.read(&header.filename[0], header.filename.size());

	uint8_t freqTableSize = 0;
	input.read(reinterpret_cast<char*>(&freqTableSize), 1);

	for (unsigned int i = 0; i < freqTableSize; i++)
	{
		char key;
		input.get(key);
		int value;
		input.read(reinterpret_cast<char*>(&value), 4);

		header.freqTable[key] = value;
	}

	return header;
}