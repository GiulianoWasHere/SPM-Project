//
// This example shows how to use the all2all (A2A) building block (BB).
// It finds the prime numbers in the range (n1,n2) using the A2A.
//
//          L-Worker --|   |--> R-Worker --|
//                     |-->|--> R-Worker --|
//          L-Worker --|   |--> R-Worker --|
//
//
//  -   Each L-Worker manages a partition of the initial range. It sends sub-partitions
//      to the R-Workers in a round-robin fashion.
//  -   Each R-Worker checks if the numbers in each sub-partition received are
//      prime by using the is_prime function
//

#include <atomic>
#include <cmath>
#include <string>
#include <vector>
#include <iostream>
#include <ff/ff.hpp>
#include <ff/all2all.hpp>
using namespace ff;
#include <utility.hpp>

struct FileStruct
{
  std::string filename;
  size_t size;
  // In this array the pointer of the blocks are stored
  size_t *sizeOfBlocks;
  unsigned char **arrayOfPointers;
};

// ------------ GLOBAL VARIBLES ---------------
std::vector<FileStruct> FilesVector;
bool compressing = false;
bool success = true;
// ------------ END GLOBAL VARIBLES ---------------

static inline bool addFileToVector(const char fname[], size_t size, const bool comp, std::vector<FileStruct> &FilesVector)
{
  const std::string infilename(fname);
  if (!comp)
  {
    if (!ends_with(infilename, ".miniz"))
      return true;
  }
  FilesVector.emplace_back(infilename, size);
  return true;
}

static inline bool walkDirff(const char dname[], const bool comp, std::vector<FileStruct> &FilesVector)
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
        if (walkDirff(file->d_name, comp, FilesVector))
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
      if (!addFileToVector(file->d_name, statbuf.st_size, comp, FilesVector))
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

static inline void usage(const char *argv0)
{
  printf("--------------------\n");
  printf("Usage: %s c|d|C|D file-or-directory L-Workers R-Workers  \n", argv0);
  printf("\nModes:\n");
  printf("c - Compresses file infile to a zlib stream into outfile\n");
  printf("d - Decompress a zlib stream from infile into outfile\n");
  printf("C - Like c but remove the input file\n");
  printf("D - Like d but remove the input file\n");
  printf("--------------------\n");
}

struct Task_t
{
  Task_t(const std::string &name) : filename(name) {}

  unsigned char *ptr;              // input pointer
  size_t size;                     // input size
  unsigned char *ptrOut = nullptr; // output pointer
  size_t cmp_size = 0;             // output size
  size_t blockid = 1;              // block identifier (for "BIG files")
  size_t nblocks = 1;              // #blocks in which a "BIG file" is split
  size_t idFile = 0;               // Id of the file in the FileVector
  size_t readBytes = 0;            // Used in the decompression to understand where each worker has to start
  size_t uncompreFileSize = 0;     // Size of the uncompressed file
  const std::string filename;      // source file name
};

static inline void printTask(Task_t *in)
{
  std::cout << "Filename: " << in->filename << std::endl;
  // std::cout << "Input Pointer: " << static_cast<void*>(in->ptr) << std::endl;
  // std::cout << "Output Pointer: " << static_cast<void*>(in->ptrOut) << std::endl;
  std::cout << "Size: " << in->size << std::endl;
  std::cout << "Cmp Size: " << in->cmp_size << std::endl;
  std::cout << "Block ID: " << in->blockid << std::endl;
  std::cout << "#Blocks: " << in->nblocks << std::endl;
  std::cout << "File ID: " << in->idFile << std::endl;
  std::cout << "readBytes: " << in->readBytes << std::endl;
  std::cout << "uncompressed File Size: " << in->uncompreFileSize << std::endl;
  std::cout << "-----------------------------" << std::endl;
}

static inline bool writeToDisk(Task_t *in, std::vector<std::atomic<int>> &vectorOfCounters)
{
  size_t idFile = in->idFile;
  size_t sizeOfT = sizeof(size_t);
  size_t nBlocks = in->nblocks;

  // Creation of the header
  unsigned char *ptrHeader = new unsigned char[sizeOfT * (in->nblocks + 2)];
  // size of file
  memcpy(ptrHeader, &in->size, sizeof(size_t));
  // number of blocks
  memcpy(ptrHeader + sizeOfT, &nBlocks, sizeof(size_t));
  for (size_t i = 0; i < nBlocks; ++i)
  {
    memcpy(ptrHeader + sizeOfT * (i + 2), &FilesVector[idFile].sizeOfBlocks[i], sizeof(size_t));
  }

  std::string outfilename = std::string(in->filename) + SUFFIX;
  FILE *pOutfile = fopen(outfilename.c_str(), "wb");
  if (!pOutfile)
  {
    if (QUITE_MODE >= 1)
    {
      perror("fopen");
      std::fprintf(stderr, "Failed opening output file %s!\n", outfilename.c_str());
      return false;
    }
  }
  // Write header
  if (fwrite(ptrHeader, 1, sizeOfT * (nBlocks + 2), pOutfile) != sizeOfT * (nBlocks + 2))
  {
    if (QUITE_MODE >= 1)
    {
      perror("fwrite");
      std::fprintf(stderr, "Failed writing to output file %s\n", outfilename.c_str());
    }
    return false;
  }
  for (size_t i = 0; i < nBlocks; ++i)
  {
    if (fwrite(FilesVector[idFile].arrayOfPointers[i], 1, FilesVector[idFile].sizeOfBlocks[i], pOutfile) != FilesVector[idFile].sizeOfBlocks[i])
    {
      if (QUITE_MODE >= 1)
      {
        perror("fwrite");
        std::fprintf(stderr, "Failed writing to output file %s\n", outfilename.c_str());
      }
      return false;
    }
  }
  if (fclose(pOutfile) != 0)
    return false;
  return true;
}
struct MultiInputHelperNode : ff::ff_minode_t<Task_t>
{
  Task_t *svc(Task_t *in)
  {
    return in;
  }
};
struct L_Worker : ff_monode_t<Task_t>
{ // must be multi-output

  L_Worker(std::vector<std::atomic<int>> &vectorOfCounters, size_t id, size_t numberOfTasks, size_t NumberOfLWorkers)
      : vectorOfCounters(vectorOfCounters), id(id), numberOfTasks(numberOfTasks), NumberOfLWorkers(NumberOfLWorkers) {}

  Task_t *svc(Task_t *in)
  {
    // IF THE INPUT IS NULL WE ARE AT THE BEGINNING AND
    // WE ARE JUST SPLITTING THE WORK BETWEEN THE WORKERS
    if (in == nullptr)
    {
      // Based on the Id (The id is given at the creation of the node)
      // the files are divided for each worker
      if (compressing) //***********COMPRESSING********
      {
        for (size_t i = 0; i < numberOfTasks; ++i)
        {
          size_t idFile = id + i * NumberOfLWorkers;
          const std::string infilename(FilesVector[idFile].filename);
          size_t infile_size = FilesVector[idFile].size;
          size_t sizeOfT = sizeof(size_t);

          unsigned char *ptr = nullptr;
          if (!mapFile(infilename.c_str(), infile_size, ptr))
          {
            std::fprintf(stderr, "Failed to mapFile\n");
            success = false;
            continue;
          }

          const size_t fullblocks = infile_size / BIGFILE_LOW_THRESHOLD;
          const size_t partialblock = infile_size % BIGFILE_LOW_THRESHOLD;
          size_t numberOfBlocks = fullblocks;

          if (partialblock)
            numberOfBlocks++;

          FilesVector[idFile].arrayOfPointers = new unsigned char *[numberOfBlocks];
          FilesVector[idFile].sizeOfBlocks = new size_t[numberOfBlocks];

          for (size_t j = 0; j < fullblocks; ++j)
          {
            Task_t *t = new Task_t(infilename);
            t->blockid = j;
            t->idFile = idFile;
            t->nblocks = numberOfBlocks;
            t->ptr = ptr;
            t->ptrOut = ptr + BIGFILE_LOW_THRESHOLD * j;
            t->size = infile_size;
            t->cmp_size = BIGFILE_LOW_THRESHOLD;
            ff_send_out(t);
          }
          if (partialblock)
          {
            Task_t *t = new Task_t(infilename);
            t->blockid = fullblocks;
            t->idFile = idFile;
            t->nblocks = numberOfBlocks;
            t->ptr = ptr;
            t->ptrOut = ptr + BIGFILE_LOW_THRESHOLD * fullblocks;
            t->size = infile_size;
            t->cmp_size = partialblock;
            ff_send_out(t);
          }
        }
      }
      else //***********DECOMPRESSING********
      {
        for (size_t i = 0; i < numberOfTasks; ++i)
        {
          size_t idFile = id + i * NumberOfLWorkers;
          const std::string infilename(FilesVector[idFile].filename);
          size_t infile_size = FilesVector[idFile].size;
          size_t sizeOfT = sizeof(size_t);

          unsigned char *ptr = nullptr;
          if (!mapFile(infilename.c_str(), infile_size, ptr))
          {
            std::fprintf(stderr, "Failed to mapFile\n");
            success = false;
            continue;
          }

          // Size of the uncompressed file
          size_t uncompressedFileSize;
          memcpy(&uncompressedFileSize, ptr, sizeof(size_t));

          // Number of blocks taken from header
          size_t numberOfBlocks;
          memcpy(&numberOfBlocks, ptr + sizeOfT, sizeof(size_t));

          unsigned char *ptrOut = new unsigned char[uncompressedFileSize];

          size_t headerSize = sizeOfT * (numberOfBlocks + 2);
          size_t bytesRead = headerSize;
          for (size_t j = 0; j < numberOfBlocks; ++j)
          {
            Task_t *t = new Task_t(infilename);
            t->blockid = j;
            t->idFile = idFile;
            t->nblocks = numberOfBlocks;
            t->ptr = ptr;
            t->ptrOut = ptrOut;
            t->uncompreFileSize = uncompressedFileSize;
            t->size = infile_size;
            t->readBytes = bytesRead;
            memcpy(&t->cmp_size, ptr + sizeOfT * (j + 2), sizeof(size_t));
            bytesRead = bytesRead + t->cmp_size;
            ff_send_out(t);
          }
        }
      }
      return EOS;
    }
    else
    {
      if (compressing)
      {
        size_t idFile = in->idFile;
        // Add the compressed block of memory to the array of pointers
        FilesVector[idFile].arrayOfPointers[in->blockid] = in->ptrOut;
        FilesVector[idFile].sizeOfBlocks[in->blockid] = in->cmp_size;
        // Using an atomic to check when all the blocks have been compressed
        int val = vectorOfCounters[idFile].fetch_add(1);

        if (val >= in->nblocks - 1)
        {

          if (!writeToDisk(in, vectorOfCounters))
          {
            std::fprintf(stderr, "Problems in the writing of the file.\n");
            success = false;
            return GO_ON;
          }
          // Cleaning memory
          for (size_t i = 0; i < in->nblocks; ++i)
          {
            delete FilesVector[idFile].arrayOfPointers[i];
          }
          delete [] FilesVector[idFile].arrayOfPointers;
          delete [] FilesVector[idFile].sizeOfBlocks;
          delete in;
          unmapFile(in->ptr, in->size);
        }
      }
      else
      {
        size_t idFile = in->idFile;
        // Using an atomic to check when all the blocks have been decompressed
        int val = vectorOfCounters[idFile].fetch_add(1);
        if (val >= in->nblocks - 1)
        {
          const std::string infilename(in->filename);
          std::string outfilename = infilename.substr(0, infilename.size() - 6);

          // if the file exist in the directory it will add 1,2,3..
          int a = 1;
          std::string tempFileName = outfilename;
          while (existsFile(tempFileName))
          {
            tempFileName = outfilename;
            size_t pos = outfilename.find(".");
            if (pos == std::string::npos)
              tempFileName = outfilename + std::to_string(a);
            else
              tempFileName = tempFileName.insert(pos, std::to_string(a));
            a++;
          }
          outfilename = tempFileName;

          bool success = writeFile(outfilename,in->ptrOut, in->uncompreFileSize);
          unmapFile(in->ptr, in->size);
          delete in->ptrOut;
          delete in;
        }
      }
      return GO_ON;
    }
  }
  std::vector<std::atomic<int>> &vectorOfCounters;
  size_t id;
  size_t numberOfTasks;
  size_t NumberOfLWorkers;
};
struct R_Worker : ff_monode_t<Task_t>
{ // must be multi-input
  R_Worker(const size_t Lw) : Lw(Lw) {}
  Task_t *svc(Task_t *in)
  {
    // SendToWriter
    // printTask(in);
    if (compressing) //***********COMPRESSING********
    {

      size_t estimation = compressBound(in->cmp_size);
      unsigned char *ptrCompress = new unsigned char[estimation];
      if (compress((ptrCompress), &estimation, in->ptrOut, in->cmp_size) != Z_OK)
      {
        if (QUITE_MODE >= 1)
          std::fprintf(stderr, "Failed to compress file in memory\n");
        success = false;
        //Cleaning memory
        unmapFile(in->ptr, in->size);
        delete[] in->ptrOut;
        return GO_ON;
      }
      in->cmp_size = estimation;
      in->ptrOut = ptrCompress;
      ff_send_out(in);
    }
    else //***********DECOMPRESSING********
    {
      //The decompression is done in the same unsigned char *, each worker won't touch the other's memory
      size_t cmp_len = BIGFILE_LOW_THRESHOLD;
      if (mz_uncompress((in->ptrOut + in->blockid * BIGFILE_LOW_THRESHOLD), &cmp_len, (const unsigned char *)(in->ptr + in->readBytes), in->cmp_size) != MZ_OK)
      {
        if (QUITE_MODE >= 1)
          std::fprintf(stderr, "Failed to decompress file in memory\n");
        success = false;
        return GO_ON;
      }
      ff_send_out(in);
    }
    return GO_ON;
  }
  const size_t Lw;
};

int main(int argc, char *argv[])
{
  if (argc < 5)
  {
    usage(argv[0]);
    return -1;
  }
  const char *pMode = argv[1];
  if (!strchr("cCdD", pMode[0]))
  {
    printf("Invalid option!\n\n");
    usage(argv[0]);
    return -1;
  }
  compressing = ((pMode[0] == 'c') || (pMode[0] == 'C'));
  REMOVE_ORIGIN = ((pMode[0] == 'C') || (pMode[0] == 'D'));

  const size_t Lw = std::stol(argv[3]);
  const size_t Rw = std::stol(argv[4]);

  struct stat statbuf;
  if (stat(argv[2], &statbuf) == -1)
  {
    perror("stat");
    fprintf(stderr, "Error: stat %s\n", argv[argc]);
    return -1;
  }
  bool dir = false;
  // std::vector<FileStruct> FilesVector;

  // Walks in the directory and add the filenames in the FileVector
  if (S_ISDIR(statbuf.st_mode))
  {
    success &= walkDirff(argv[2], compressing, FilesVector);
  }
  else
  {
    success &= addFileToVector(argv[2], statbuf.st_size, compressing, FilesVector);
  }

  std::vector<std::atomic<int>> vectorOfCounters(FilesVector.size());

  std::vector<ff_node *> LW;
  std::vector<ff_node *> RW;
  size_t numberTasks = FilesVector.size() / Lw;
  size_t overflowTasks = FilesVector.size() % Lw;
  for (size_t i = 0; i < Lw; ++i)
  {
    if (overflowTasks)
    {
      LW.push_back(new ff::ff_comb(new MultiInputHelperNode, new L_Worker(vectorOfCounters, i, numberTasks + 1, Lw)));
      overflowTasks--;
    }
    else
      LW.push_back(new ff::ff_comb(new MultiInputHelperNode, new L_Worker(vectorOfCounters, i, numberTasks, Lw)));
  }
  for (size_t i = 0; i < Rw; ++i)
    RW.push_back(new ff::ff_comb(new MultiInputHelperNode, new R_Worker(Lw)));

  // Adding Lworkers and Rworkers to a2a
  ff_a2a a2a;
  a2a.add_firstset(LW);
  a2a.add_secondset(RW);
  a2a.wrap_around();

  
  
  if (a2a.run_and_wait_end() < 0)
  {
    error("running a2a\n");
    return -1;
  }

  if (!success)
  {
    printf("Exiting with (some) Error(s)\n");
    return -1;
  }
  printf("Exiting with Success\n");
  return 0;
}