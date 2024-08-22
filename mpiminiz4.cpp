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
#include <omp.h>

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
std::vector<int> vectorOfCounters;
bool compressing = false;
bool success = true;
int MAX_FILES_IN_DIRECTORY = 2000;
int myId;
int numP;
// ------------ END GLOBAL VARIBLES ---------------

struct Task_t
{
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

static inline bool mpiMaster(size_t i, int numP)
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
    return false;
  }

  const size_t fullblocks = infile_size / BIGFILE_LOW_THRESHOLD;
  const size_t partialblock = infile_size % BIGFILE_LOW_THRESHOLD;
  size_t numberOfBlocks = fullblocks;
  if (partialblock)
    numberOfBlocks++;

  std::cout << infilename.c_str() << "\n";
  std::cout << omp_get_thread_num() << "\n";

  std::vector<int> counts(numP);
  std::vector<int> displs(numP);
  for (int j = 0; j < numP; ++j)
  {
    auto start = (fullblocks * j / numP) * BIGFILE_LOW_THRESHOLD;
    auto end = (fullblocks * (j + 1) / numP) * BIGFILE_LOW_THRESHOLD;
    counts[j] = end - start;
    displs[j] = start;
  }
  counts[counts.size() - 1] += partialblock;

  MPI_Request rq_send[numberOfBlocks];
  MPI_Status statuses[numberOfBlocks];
  for (int j = 1; j < numP; ++j)
  {
    std::cout << "SIZE OF counts:" << counts[j] << "\n";
    if (counts[j] != 0)
      MPI_Isend((ptr + displs[j]), counts[j], MPI_UNSIGNED_CHAR, j, idFile, MPI_COMM_WORLD, &rq_send[j]);
  }
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

  L_Worker(int myId, int numP)
      : myId(myId), numP(numP) {}

  Task_t *svc(Task_t *in)
  {
    if (compressing) //***********COMPRESSING********
    {
      MPI_Status status;
      MPI_Request rq_recv;
      do
      {

        MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == INT_MAX)
          break;

        int mpitag = status.MPI_TAG;
        std::cout << "SIZE OF THE FILE:" << FilesVector[mpitag].size << "\n";
        // Get an estimate of the data to recive
        int estimation = FilesVector[mpitag].size / numP + BIGFILE_LOW_THRESHOLD;
        std::cout << "estimation:" << estimation << "\n";
        unsigned char *ptrIN = new unsigned char[estimation];
        MPI_Irecv(ptrIN, estimation, MPI_UNSIGNED_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &rq_recv);
        MPI_Wait(&rq_recv, &status);
        int countElements;
        MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &countElements);

        std::cout << myId << "RECIVED: " << ptrIN[0] << " MY COUNT IS :" << countElements << "\n";
        size_t idFile = mpitag;

        size_t infile_size = countElements;
        size_t sizeOfT = sizeof(size_t);

        unsigned char *ptr = ptrIN;

        const size_t fullblocks = infile_size / BIGFILE_LOW_THRESHOLD;
        const size_t partialblock = infile_size % BIGFILE_LOW_THRESHOLD;
        size_t numberOfBlocks = fullblocks;

        if (partialblock)
          numberOfBlocks++;

        FilesVector[idFile].arrayOfPointers = new unsigned char *[numberOfBlocks];
        FilesVector[idFile].sizeOfBlocks = new size_t[numberOfBlocks];

        for (size_t j = 0; j < fullblocks; ++j)
        {
          Task_t *t = new Task_t;
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
          Task_t *t = new Task_t;
          t->blockid = fullblocks;
          t->idFile = idFile;
          t->nblocks = numberOfBlocks;
          t->ptr = ptr;
          t->ptrOut = ptr + BIGFILE_LOW_THRESHOLD * fullblocks;
          t->size = infile_size;
          t->cmp_size = partialblock;
          ff_send_out(t);
        }
      } while (status.MPI_TAG != INT_MAX);
    }
    else //***********DECOMPRESSING********
    {
      /*for (size_t i = 0; i < numberOfTasks; ++i)
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
      }*/
    }
    return EOS;
  }
  int myId;
  int numP;
};
struct R_Worker : ff_minode_t<Task_t>
{ // must be multi-input
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
        // Cleaning memory
        // unmapFile(ptr, infile_size);
        // delete[] ptrOut;
        return GO_ON;
      }
      in->cmp_size = estimation;
      in->ptrOut = ptrCompress;
      ff_send_out(in);
    }
    else //***********DECOMPRESSING********
    {
      // The decompression is done in the same unsigned char *, each worker won't touch the other's memory
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
};

struct Gatherer : ff_minode_t<Task_t>
{ // must be multi-input
  Task_t *svc(Task_t *in)
  {
    if (compressing)
    {
      size_t idFile = in->idFile;
      // Add the compressed block of memory to the array of pointers
      FilesVector[idFile].arrayOfPointers[in->blockid] = in->ptrOut;
      FilesVector[idFile].sizeOfBlocks[in->blockid] = in->cmp_size;

      int val = vectorOfCounters[idFile]++;

      if (val >= in->nblocks - 1)
      {

        // WRITE TO MASTER
        std::cout << "SONO: " << myId << "HO COMPRESSO:" << "\n";
        for (int i = 0; i < in->nblocks; ++i)
          std::cout << FilesVector[idFile].sizeOfBlocks[i] << "\n";
        // Cleaning memory
        for (size_t i = 0; i < in->nblocks; ++i)
        {
          delete FilesVector[idFile].arrayOfPointers[i];
        }
        delete FilesVector[idFile].sizeOfBlocks;
      }
    }
    else
    {
      size_t idFile = in->idFile;
      // Using an atomic to check when all the blocks have been decompressed
      int val = vectorOfCounters[idFile]++;
      if (val >= in->nblocks - 1)
      {
        // Send BLOCK

        unmapFile(in->ptr, in->size);
        delete[] in->ptrOut;
      }
    }
    return GO_ON;
  }
};
static inline bool mpiWorker(int myId, int numP)
{
  // This part read the first message where the length of each file is specified
  //---------------
  unsigned long long array[MAX_FILES_IN_DIRECTORY];
  MPI_Status status;
  MPI_Status statusProbe;
  int numberOfFiles = 0;
  MPI_Request rq_recv;
  MPI_Irecv(array, MAX_FILES_IN_DIRECTORY * sizeof(MPI_UNSIGNED_LONG_LONG), MPI_UNSIGNED_LONG_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &rq_recv);
  MPI_Wait(&rq_recv, &status);
  MPI_Get_count(&status, MPI_UNSIGNED_LONG_LONG, &numberOfFiles);

  int countElements;
  do
  {

    MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    if (status.MPI_TAG == INT_MAX)
      break;
    std::cout << "SIZE OF THE FILE:" << array[status.MPI_TAG] << "\n";
    // Get an estimate of the data to recive
    int estimation = array[status.MPI_TAG] / numP + BIGFILE_LOW_THRESHOLD;
    std::cout << "estimation:" << estimation << "\n";
    unsigned char *ptrIN = new unsigned char[estimation];
    MPI_Irecv(ptrIN, estimation, MPI_UNSIGNED_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &rq_recv);
    MPI_Wait(&rq_recv, &status);
    MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &countElements);

    std::cout << myId << "RECIVED: " << ptrIN[0] << " MY COUNT IS :" << countElements << "\n";

    // std::cout << myId << "RECIVED: " << ptrIN[0] << " MY COUNT IS :" << countElements << "\n";
  } while (status.MPI_TAG != INT_MAX);

  std::cout << myId << "FINE" << "\n";
  return true;
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
  /* int myId;
  int numP; */
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

  // Handle of the master
  if (!myId)
  {
    // Walks in the directory and add the filenames in the FileVector
    if (S_ISDIR(statbuf.st_mode))
    {
      success &= walkDirMpi(argv[2], compressing, FilesVector);
    }
    else
    {
      success &= addFileToVector(argv[2], statbuf.st_size, compressing, FilesVector);
    }

    size_t sizeVector = FilesVector.size();
    unsigned long long arrayToSend[sizeVector];

    // Send the size of each file to every worker
    //------------------------------------------
    for (int i = 0; i < sizeVector; ++i)
    {
      arrayToSend[i] = FilesVector[i].size;
      if (arrayToSend[i] != FilesVector[i].size)
        std::cout << "NOT EQUAL NOT EQUAL NOT EQUAL FILESVECTOR ARRAY TO SEND" << "\n";
    }
    MPI_Request rq_sendBEGINNING[numP];
    for (int i = 1; i < numP; ++i)
    {
      MPI_Isend(arrayToSend, sizeVector, MPI_UNSIGNED_LONG_LONG, i, 0, MPI_COMM_WORLD, &rq_sendBEGINNING[i]);
    }

    //------------------------------------------
#pragma omp parallel for
    for (int i = 0; i < sizeVector; ++i)
    {
      if (FilesVector[i].size > BIGFILE_LOW_THRESHOLD)
      {
        mpiMaster(i, numP);
      }
      else // In case the files are very small we just do it locally
      {
        if (compressing)
          compressFile(FilesVector[i].filename.c_str(), FilesVector[i].size, 0);
        else
          decompressFile(FilesVector[i].filename.c_str(), FilesVector[i].size, 0);
      }
    }

    // Send messages to the workers to stop them
    MPI_Request rq_end[numP];
    MPI_Status rq_end_status[numP];
    for (int j = 1; j < numP; ++j)
    {
      MPI_Isend(NULL, 0, MPI_UNSIGNED_CHAR, j % (numP - 1) + 1, INT_MAX, MPI_COMM_WORLD, &rq_end[j]);
    }
    sleep(10);
    // MPI_Waitall(numP,rq_end,rq_end_status);
  }
  else // Handle the workers
  {
    mpiWorker(myId, numP);
  }
  if (!success)

  // END
  {
    printf("Exiting with (some) Error(s)\n");
    return -1;
  }
  if (!myId)
    printf("Exiting with Success\n");
  MPI_Finalize();
  return 0;
}