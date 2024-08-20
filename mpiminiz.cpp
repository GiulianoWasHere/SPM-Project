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
#include <mpi.h>

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

static inline bool walkDirMpi(const char dname[], const bool comp, std::vector<FileStruct> &FilesVector)
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
        if (walkDirMpi(file->d_name, comp, FilesVector))
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

int main(int argc, char *argv[])
{
  int provided;
  int flag;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Is_thread_main(&flag);
  if (!flag)
  {
    std::printf("This thread called MPI_Init_thread but it is not the main thread\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
    return -1;
  }
  int claimed;
  MPI_Query_thread(&claimed);
  if (claimed != provided)
  {
    std::printf("MPI_THREAD_MULTIPLE not provided\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
    return -1;
  }
  int numP;
  int myId;

  // Get the number of processes
  MPI_Comm_rank(MPI_COMM_WORLD, &myId);
  MPI_Comm_size(MPI_COMM_WORLD, &numP);

  if (argc < 5)
  {
    usage(argv[0]);
    MPI_Abort(MPI_COMM_WORLD, -1);
    return -1;
  }
  const char *pMode = argv[1];
  if (!strchr("cCdD", pMode[0]))
  {
    printf("Invalid option!\n\n");
    usage(argv[0]);
    MPI_Abort(MPI_COMM_WORLD, -1);
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
    MPI_Abort(MPI_COMM_WORLD, -1);
    return -1;
  }
  bool dir = false;
  // std::vector<FileStruct> FilesVector;

  // Walks in the directory and add the filenames in the FileVector
  if (!myId)
  {
    if (S_ISDIR(statbuf.st_mode))
    {
      success &= walkDirMpi(argv[2], compressing, FilesVector);
    }
    else
    {
      success &= addFileToVector(argv[2], statbuf.st_size, compressing, FilesVector);
    }

    size_t sizeVector = FilesVector.size();
#pragma omp parallel for
    for (int i = 0; i < sizeVector; ++i)
    {
      size_t idFile = i;
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

      MPI_UNSIGNED_CHAR;

      const size_t fullblocks = infile_size / BIGFILE_LOW_THRESHOLD;
      const size_t partialblock = infile_size % BIGFILE_LOW_THRESHOLD;
      size_t numberOfBlocks = fullblocks;
      if (partialblock)
        numberOfBlocks++;

      std::vector<int> counts(numP);
      std::vector<int> displs(numP);
      for (int j = 0; j < numP; ++j)
      {
        auto start = (fullblocks * j / numP);
        auto end = (fullblocks * (j+1) / numP);
        counts[j] = end - start;
        displs[j] = start;
      }
      size_t count = 0;
      for (int j = 0; j < counts.size(); j++)
      {
        std::cout << counts[j] * BIGFILE_LOW_THRESHOLD << std::endl;
        count += counts[j]* BIGFILE_LOW_THRESHOLD;
      }
      counts[counts.size()-1] += partialblock;
      count += partialblock;
      std::cout << "COUNT:" << count << std::endl;
      std::cout << "FILE SIZE:" << infile_size << std::endl;
      for (int j = 0; j < displs.size(); j++)
      {
        std::cout << displs[j] * BIGFILE_LOW_THRESHOLD << std::endl;
      }
      std::cout << infilename.c_str() << "\n";


      /*MPI_Scatterv(ptr, 	
				 counts.data(), 
				 displs.data(),
				 MPI_DOUBLE,
				 localA.data(), // rcvbuf
				 localn,	    // rcvcount 
				 MPI_DOUBLE,	
				 0,	      	   // root 
				 MPI_COMM_WORLD);*/

    }
  }
  else
  {
    // std::cout << myId << "\n";
  }
  if (!success)
  {
    printf("Exiting with (some) Error(s)\n");
    return -1;
  }
  if (!myId)
    printf("Exiting with Success\n");
  MPI_Finalize();
  return 0;
}