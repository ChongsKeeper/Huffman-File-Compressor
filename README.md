# Huffman-File-Compressor

Command line based Huffman compressor. 

# Intent

A functional CLI11 command line based compression program using the Huffman tree algorithm.

# Commands

-d          Decompress  
-p, --path  Optional. File path  
-o          Overwrite. Force program to overwrite existing file if the output file already exists.  
-l          List header contents. For compressed files  
-k          Keep file. Used for debugging. If file integrity fails, don't delete it.  

# Scope

Huffman algorithm to encode and decode an input stream.  
Files compressed by the program are written with a header containing the frequency table, file name, compressed and uncompressed size, and an MD5 hash to verify file integrity.

# Input

Command line arguments and a file name.

# Output

If compressing, a compressed file ending in .huf  
If decompressing, the orignal file.
