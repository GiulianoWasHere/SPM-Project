#if !defined _UTILITY_HPP
#define _UTILITY_HPP

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <ftw.h>

#include <algorithm>
#include <string>
#include <stdexcept>

#include <miniz/miniz.h>

#include <iostream>
#include <chrono>

#define SUFFIX ".miniz"
#define BUF_SIZE (1024 * 1024)

// global variables with their default values -------------------------------------------------
static bool comp = true;					   // by default, it compresses
//static size_t BIGFILE_LOW_THRESHOLD = 1048576; // 1Mbyte
static size_t BIGFILE_LOW_THRESHOLD = 2097152; // 2Mbytes
//static size_t BIGFILE_LOW_THRESHOLD = 3145728; // 3Mbyte
//static size_t BIGFILE_LOW_THRESHOLD = 4194304; // 4Mbyte
//static size_t BIGFILE_LOW_THRESHOLD = 5242880; // 5Mbyte
//static size_t BIGFILE_LOW_THRESHOLD = 6291456; // 6Mbyte
//static size_t BIGFILE_LOW_THRESHOLD = 8388608; // 8Mbyte
//static size_t BIGFILE_LOW_THRESHOLD = 16777216; // 16Mbyte 
//static size_t BIGFILE_LOW_THRESHOLD = 33554432; // 32Mbyte 
//static size_t BIGFILE_LOW_THRESHOLD = 67108864; // 64Mbyte 
//static size_t BIGFILE_LOW_THRESHOLD = 134217728; // 128Mbyte 
static bool REMOVE_ORIGIN = false;			   // Does it keep the origin file? NOT USED
static int QUITE_MODE = 1;					   // 0 silent, 1 only errors, 2 everything
static bool RECUR = false;					   // do we have to process the contents of subdirs? NOT USED
// --------------------------------------------------------------------------------------------

// map the file pointed by filepath in memory
// if size is zero, it looks for file size
// if everything is ok, it returns the memory pointer ptr
static inline bool mapFile(const char fname[], size_t &size, unsigned char *&ptr)
{
	// open input file.
	int fd = open(fname, O_RDONLY);
	if (fd < 0)
	{
		if (QUITE_MODE >= 1)
		{
			perror("mapFile open");
			std::fprintf(stderr, "Failed opening file %s\n", fname);
		}
		return false;
	}
	if (size == 0)
	{
		struct stat s;
		if (fstat(fd, &s))
		{
			if (QUITE_MODE >= 1)
			{
				perror("fstat");
				std::fprintf(stderr, "Failed to stat file %s\n", fname);
			}
			return false;
		}
		size = s.st_size;
	}

	// map all the file in memory
	ptr = (unsigned char *)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED)
	{
		if (QUITE_MODE >= 1)
		{
			perror("mmap");
			std::fprintf(stderr, "Failed to memory map file %s\n", fname);
		}
		return false;
	}
	close(fd);
	return true;
}
// unmap a previously memory-mapped file
static inline void unmapFile(unsigned char *ptr, size_t size)
{
	if (munmap(ptr, size) < 0)
	{
		if (QUITE_MODE >= 1)
		{
			perror("nummap");
			std::fprintf(stderr, "Failed to unmap file\n");
		}
	}
}
// write size bytes starting from ptr into filename
static inline bool writeFile(const std::string &filename, unsigned char *ptr, size_t size)
{
	FILE *pOutfile = fopen(filename.c_str(), "wb");
	if (!pOutfile)
	{
		if (QUITE_MODE >= 1)
		{
			perror("fopen");
			std::fprintf(stderr, "Failed opening output file %s!\n", filename.c_str());
		}
		return false;
	}
	if (fwrite(ptr, 1, size, pOutfile) != size)
	{
		if (QUITE_MODE >= 1)
		{
			perror("fwrite");
			std::fprintf(stderr, "Failed writing to output file %s\n", filename.c_str());
		}
		return false;
	}
	if (fclose(pOutfile) != 0)
		return false;
	return true;
}

// check if dir is '.' or '..'
static inline bool isdot(const char dir[])
{
	int l = strlen(dir);
	if ((l > 0 && dir[l - 1] == '.'))
		return true;
	return false;
}

// If compdecomp is true (we are compressing), it checks if fname has the suffix SUFFIX,
// if yes it returns true
// If compdecomp is false (we are decompressing), it checks if fname has the suffix SUFFIX,
// if yes it returns false
static inline bool discardIt(const char *fname, const bool compdecomp)
{
	const int lensuffix = strlen(SUFFIX);
	const int len = strlen(fname);
	if (len > lensuffix &&
		(strncmp(&fname[len - lensuffix], SUFFIX, lensuffix) == 0))
	{
		return compdecomp; // true or false depends on we are compressing or decompressing;
	}
	return !compdecomp;
}

// check if the string 's' is a number, otherwise it returns false
static bool isNumber(const char *s, long &n)
{
	try
	{
		size_t e;
		n = std::stol(s, &e, 10);
		return e == strlen(s);
	}
	catch (const std::invalid_argument &)
	{
		return false;
	}
	catch (const std::out_of_range &)
	{
		return false;
	}
}
static inline char *getOption(char **begin, char **end, const std::string &option)
{
	char **itr = std::find(begin, end, option);
	if (itr != end && ++itr != end)
		return *itr;
	return nullptr;
}
// create a tempory "unique" directory name
static inline bool createTmpDir(std::string &tmpdir)
{
	tmpdir += "tmpdir.XXXXXX";
	char *tpl = const_cast<char *>(tmpdir.c_str());
	if (mkdtemp(tpl) == nullptr)
	{
		if (QUITE_MODE >= 1)
		{
			perror("mkdtemp");
			std::fprintf(stderr, "Error: cannot create tmp dir %s\n", tmpdir.c_str());
		}
		return false;
	}
	return true;
}

// remove a file, it is used as a callback by removeDir
static int removeFile(const char *name, const struct stat *st = nullptr, int x = 0, struct FTW *ftw = nullptr)
{
	if (unlink(name) == -1)
	{
		if (QUITE_MODE >= 1)
		{
			perror("unlink");
			std::fprintf(stderr, "Error: cannot remove file %s\n", name);
		}
		return -1;
	}
	return 0;
}
// remove tmpdir; if force, first removes all files and then the directory
static inline bool removeDir(const std::string &tmpdir, bool force = false)
{

	if (rmdir(tmpdir.c_str()) == -1)
	{
		if (force && (errno == ENOTEMPTY || errno == EEXIST))
		{
			// Delete the directory and its contents by traversing the tree in reverse order,
			// without crossing mount boundaries and symbolic links
			if (nftw(tmpdir.c_str(), removeFile, 16, FTW_DEPTH | FTW_MOUNT | FTW_PHYS) < 0)
			{
				if (QUITE_MODE >= 1)
				{
					perror("mkdtemp");
					std::fprintf(stderr, "Error: cannot create tmp dir %s\n", tmpdir.c_str());
				}
				return false;
			}
			return true;
		}
		if (QUITE_MODE >= 1)
		{
			perror("rmdir");
			std::fprintf(stderr, "Error: cannot remove tmp dir %s\n", tmpdir.c_str());
		}
		return false;
	}
	return true;
}

// --------------------------------------------------------------------------

static inline int compressFile(const char fname[], size_t infile_size,
							   const bool removeOrigin = REMOVE_ORIGIN)
{
	// define the output file name
	const std::string infilename(fname);
	std::string outfilename = std::string(fname) + SUFFIX;

	size_t sizeOfT = sizeof(size_t);
	unsigned char *ptr = nullptr;
	if (!mapFile(fname, infile_size, ptr))
		return -1;

	const size_t fullblocks = infile_size / BIGFILE_LOW_THRESHOLD;
	const size_t partialblock = infile_size % BIGFILE_LOW_THRESHOLD;

	

	size_t headerSize = 0;
	// Add one long for the size of the uncompressed file
	headerSize += sizeOfT;
	// long for the number of blocks
	headerSize += sizeOfT;
	// size of each compressed block
	size_t numberOfBlocks = fullblocks;
	if (partialblock)
	{
		headerSize += sizeOfT * (fullblocks + 1);
		numberOfBlocks++;
	}
	else
		headerSize += sizeOfT * fullblocks;

	// Estimate of the length of the compressed file
	unsigned long compressedFileLength = mz_compressBound(BIGFILE_LOW_THRESHOLD) * numberOfBlocks;
	// add the header size
	compressedFileLength += headerSize;
	unsigned char *ptrOut = new unsigned char[compressedFileLength];

	//std::fprintf(stderr, "Number of blocks : %zu \n", numberOfBlocks);
	//std::fprintf(stderr, "infile_size: %zu \n", infile_size);

	// size of file
	memcpy(ptrOut, &infile_size, sizeof(size_t));
	// number of blocks
	memcpy(ptrOut + sizeOfT, &numberOfBlocks, sizeof(size_t));

	// std::fprintf(stderr, "Number of blocks2 : %zu \n\n", numberOfBlocks2);
	// std::fprintf(stderr, "infile size : %zu \n",*ptrOut);
	// std::fprintf(stderr, "blocks numers: %zu \n\n",*(ptrOut + sizeof(size_t)));
	//std::fprintf(stderr, "header size : %zu \n\n", headerSize);

	// Total bytes written after the header
	size_t tot = headerSize;

	for (size_t i = 0; i < fullblocks; ++i)
	{
		size_t cmp_len = compressBound(BIGFILE_LOW_THRESHOLD);
		if (compress((ptrOut + tot), &cmp_len, (const unsigned char *)(ptr + BIGFILE_LOW_THRESHOLD * i), BIGFILE_LOW_THRESHOLD) != Z_OK)
		{
			if (QUITE_MODE >= 1)
				std::fprintf(stderr, "Failed to compress file in memory\n");
			// Cleaning memory
			unmapFile(ptr, infile_size);
			delete[] ptrOut;
			return -1;
		}
		tot += cmp_len;
		// Putting on the header the compressed dimension of the block
		memcpy(ptrOut + sizeOfT * (i + 2), &cmp_len, sizeof(size_t));

		// size_t blocksize;
		// memcpy(&blocksize,ptrOut + sizeOfT * (i+1),sizeof(size_t));
		// std::fprintf(stderr, "blocksize : %zu \n",blocksize);
	}
	if (partialblock)
	{
		size_t cmp_len = compressBound(partialblock);
		if (compress((ptrOut + tot), &cmp_len, (const unsigned char *)(ptr + BIGFILE_LOW_THRESHOLD * fullblocks), partialblock) != Z_OK)
		{
			if (QUITE_MODE >= 1)
				std::fprintf(stderr, "Failed to compress file in memory\n");
			// Cleaning memory
			unmapFile(ptr, infile_size);
			delete[] ptrOut;
			return -1;
		}

		tot += cmp_len;

		//size_t blocksize;
		//std::fprintf(stderr, "len chunk : %zu \n", cmp_len);

		//memcpy(&blocksize, ptrOut + sizeOfT * (fullblocks + 2), sizeof(size_t));
		//std::fprintf(stderr, "before blocksize : %zu \n", blocksize);


		// Putting on the header the compressed dimension of the block
		memcpy(ptrOut + sizeOfT * (fullblocks + 2), &cmp_len, sizeof(size_t));

		//memcpy(&blocksize, ptrOut + sizeOfT * (fullblocks + 2), sizeof(size_t));
		//std::fprintf(stderr, "blocksize : %zu \n", blocksize);
	}

	//numberOfBlocks2 = 0;
	//memcpy(&numberOfBlocks2, ptrOut + sizeOfT, sizeof(size_t));

	//std::fprintf(stderr, "Number of blocks2 : %zu \n\n", numberOfBlocks2);

	// write the compressed data into disk
	bool success = writeFile(outfilename, ptrOut, tot);
	if (success && removeOrigin)
	{
		removeFile(fname);
	}
	unmapFile(ptr, infile_size);
	delete[] ptrOut;
	return 0;
}

// returns -1 fatal error, 0 not valid header, 1 valid header
static inline int checkHeader(const char fname[])
{

	unsigned char s_inbuf[256];
	unsigned char s_outbuf[256];
	FILE *pInfile;
	// Open input file.
	pInfile = fopen(fname, "rb");
	if (!pInfile)
		return -1;

	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	stream.next_out = s_outbuf;
	stream.avail_out = 256;

	if (inflateInit(&stream))
	{
		fclose(pInfile);
		return -1;
	}
	size_t n = 12; // this is the minimum size of a compressed file
	if (fread(s_inbuf, 1, n, pInfile) != n)
	{
		fclose(pInfile);
		return -1;
	}
	stream.next_in = s_inbuf;
	stream.avail_in = n;
	int status = inflate(&stream, Z_SYNC_FLUSH);
	fclose(pInfile);
	inflateEnd(&stream);
	if (status == Z_STREAM_END || status == Z_OK)
		return 1;
	return 0;
}


static inline bool ends_with(const std::string& str, const std::string& suffix)
{
  return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}

inline bool existsFile (const std::string& name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}
static inline int decompressFile(const char fname[], size_t infile_size,
								 const bool removeOrigin = REMOVE_ORIGIN)
{
	// define the output file name
	const std::string infilename(fname);

	// If the file doesn't end with zip it skips the file
	if(!ends_with(infilename,SUFFIX)) return 0;

	//Remove the .miniz
	std::string outfilename = infilename.substr(0, infilename.size()-6);

	//if the file exist in the directory it will add 1,2,3..
	int a = 1;
	std::string tempFileName = outfilename;
	while(existsFile(tempFileName))
	{
		tempFileName = outfilename;
		size_t pos = outfilename.find(".");
		if(pos==std::string::npos)
			tempFileName = outfilename + std::to_string(a);
		else
			tempFileName = tempFileName.insert(pos,std::to_string(a));
		a++;
	}
	outfilename = tempFileName;

	size_t sizeOfT = sizeof(size_t);
	unsigned char *ptr = nullptr;
	if (!mapFile(fname, infile_size, ptr))
		return -1;
	
	// Size of the uncompressed file taken from the header
	size_t uncompressedFileSize;
	memcpy(&uncompressedFileSize, ptr, sizeof(size_t));

	// Number of blocks taken from header
	size_t numberOfBlocks;
	memcpy(&numberOfBlocks, ptr + sizeOfT, sizeof(size_t));

	unsigned char *ptrOut = new unsigned char[uncompressedFileSize];

	size_t headerSize = sizeOfT * (numberOfBlocks + 2);
	// Total bytes written
	size_t tot = 0;
	// Total of bytes read after the header
	size_t readBytes = headerSize;
	for (size_t i = 0; i < numberOfBlocks; ++i)
	{
		size_t sizeUncompBlock;
		//Get the size of the block from the header of the file
		memcpy(&sizeUncompBlock, ptr + sizeOfT * (i + 2), sizeof(size_t));

		unsigned long cmp_len = BIGFILE_LOW_THRESHOLD + BIGFILE_LOW_THRESHOLD;
		if (uncompress((ptrOut + tot), &cmp_len, (const unsigned char *)(ptr + readBytes), sizeUncompBlock) != MZ_OK)
		{
			if (QUITE_MODE >= 1)
				std::fprintf(stderr, "Failed to compress file in memory\n");
				// Cleaning memory
				unmapFile(ptr, infile_size);
				delete[] ptrOut;
			return -1;
		}
		readBytes += sizeUncompBlock;
		tot += cmp_len;
	}

	// write the compressed data into disk
	bool success = writeFile(outfilename, ptrOut, tot);
	if (success && removeOrigin)
	{
		removeFile(fname);
	}
	unmapFile(ptr, infile_size);
	delete[] ptrOut;
	return 0;
}

// --------------------------------------------------------------------------

// returns false in case of error
static inline bool doWork(const char fname[], size_t size, const bool comp)
{
	if (comp)
	{
		if (compressFile(fname, size, REMOVE_ORIGIN) < 0)
			return false;
	}
	else
	{
		if (decompressFile(fname, size, REMOVE_ORIGIN) < 0)
			return false;
	}
	return true;
}

// returns false in case of error
static inline bool walkDir(const char dname[], const bool comp)
{
	if (chdir(dname) == -1)
	{
		if (QUITE_MODE >= 1)
		{
			perror("chdir");
			std::fprintf(stderr, "Error: chdir %s\n", dname);
		}
		return false;
	}
	DIR *dir;
	if ((dir = opendir(".")) == NULL)
	{
		if (QUITE_MODE >= 1)
		{
			perror("opendir");
			std::fprintf(stderr, "Error: opendir %s\n", dname);
		}
		return false;
	}
	struct dirent *file;
	bool error = false;
	while ((errno = 0, file = readdir(dir)) != NULL)
	{
		struct stat statbuf;
		if (stat(file->d_name, &statbuf) == -1)
		{
			if (QUITE_MODE >= 1)
			{
				perror("stat");
				std::fprintf(stderr, "Error: stat %s\n", file->d_name);
			}
			return false;
		}
		if (S_ISDIR(statbuf.st_mode))
		{
			if (!isdot(file->d_name))
			{
				if (walkDir(file->d_name, comp))
				{
					if (!chdir(".."))
					{
						perror("chdir");
						std::fprintf(stderr, "Error: chdir ..\n");
						error = true;
					}
				}
				else
					error = true;
			}
		}
		else
		{
			if (!doWork(file->d_name, statbuf.st_size, comp))
				error = true;
		}
	}
	if (errno != 0)
	{
		if (QUITE_MODE >= 1)
			perror("readdir");
		error = true;
	}
	closedir(dir);
	return !error;
}

#endif
