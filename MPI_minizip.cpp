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
#include <filesystem>
struct FileStruct
{
  std::string filename;
  size_t size;
  int uncompressedLength = 0;
  // In this array the pointer of the blocks are stored
  unsigned char *pointer;
  size_t *sizeOfBlocks;
  unsigned char **arrayOfPointers;
  size_t compressedLength = 0;
  int numBlocks = -1; // Used in decompressing to check if it is the first message from the Master
};

// ------------ GLOBAL VARIBLES ---------------
std::vector<FileStruct> FilesVector;
std::vector<int> vectorOfCounters;
bool compressing = false;
bool success = true;
int MAX_FILES_IN_DIRECTORY = 2000;
int myId;
int numP;
int numW;
bool workingMaster = false;
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
  printf("Usage: %s c|d|C|D file-or-directory Farm-Workers  \n", argv0);
  printf("\nModes:\n");
  printf("c - Compresses file infile to a zlib stream into outfile\n");
  printf("d - Decompress a zlib stream from infile into outfile\n");
  printf("--------------------\n");
}

static inline bool mpiMasterCompressing(size_t i, int numP)
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

  std::vector<int> counts(numW);
  std::vector<int> displs(numW);
  int max = 0;

  // Here we split the data between the other nodes, we send X nodes to each node in one message
  for (int j = 0; j < numW; ++j)
  {
    auto start = (fullblocks * j / (numW)) * BIGFILE_LOW_THRESHOLD;
    auto end = (fullblocks * (j + 1) / (numW)) * BIGFILE_LOW_THRESHOLD;
    counts[j] = end - start;
    displs[j] = start;

    if (counts[j] > max)
      max = counts[j] + partialblock;
  }
  counts[counts.size() - 1] += partialblock;

  MPI_Request rq_send[numW];
  MPI_Status statuses[numW];
  int loopLength = (numW == numP) ? numP : numW;
  int sentMessages = 0;
  // Send the data to the workers
  for (int j = (numW == numP) ? 1 : 0; j < loopLength; ++j)
  {
    if (counts[j] != 0)
    {
      MPI_Isend((ptr + displs[j]), counts[j], MPI_UNSIGNED_CHAR, j + numP - numW, idFile, MPI_COMM_WORLD, &rq_send[j]);
      sentMessages++;
    }
  }

  // This vector is used to
  FilesVector[idFile].arrayOfPointers = new unsigned char *[numW];
  int activeWorkers[numW];
  for (int j = 0; j < numW; ++j)
  {
    activeWorkers[j] = -1;
  }
  if (!workingMaster)
  {
    size_t sizeOfT = sizeof(size_t);
    // size of the first 2 sizeof t in the header
    size_t compressFileSize = sizeOfT * 2;
    size_t compressedByWorkerSize[numW];
    // Some nodes may not receive any data to process so we use sent messages
    for (int j = 0; j < sentMessages; ++j)
    {
      MPI_Request rq_recv;
      MPI_Status status;
      // We estimate the data to receive
      int estimatedSize = max + BIGFILE_LOW_THRESHOLD * 4;
      unsigned char *ptrIN = new unsigned char[estimatedSize];
      MPI_Irecv(ptrIN, estimatedSize, MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE, idFile, MPI_COMM_WORLD, &rq_recv);
      MPI_Wait(&rq_recv, &status);
      int countElements;
      MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &countElements);
      size_t nblocks;

      memcpy(&nblocks, ptrIN, sizeOfT);
      // we keep track of the size of the compressed file
      compressFileSize += countElements - sizeOfT;
      // Number of bytes compressed by the worker
      compressedByWorkerSize[status.MPI_SOURCE - 1] = countElements - sizeOfT * (nblocks + 1);
      // Use this variable to know which workers are active and the number of blocks they have compressed
      activeWorkers[status.MPI_SOURCE - 1] = nblocks;
      // store the pointer
      FilesVector[idFile].arrayOfPointers[status.MPI_SOURCE - 1] = ptrIN;
    }

    //  Creation header
    unsigned char *ptrHeader = new unsigned char[sizeOfT * (numberOfBlocks + 2)];

    // size of file
    memcpy(ptrHeader, &FilesVector[idFile].size, sizeof(size_t));
    // number of blocks
    memcpy(ptrHeader + sizeOfT, &numberOfBlocks, sizeof(size_t));

    size_t bytesRead = sizeOfT * 2;
    for (int j = 0; j < numW; ++j)
    {
      // write the length of each block in the header
      if (activeWorkers[j] != -1)
      {
        memcpy((ptrHeader + bytesRead), (FilesVector[idFile].arrayOfPointers[j] + sizeOfT), activeWorkers[j] * sizeOfT);
        bytesRead += activeWorkers[j] * sizeOfT;
      }
    }

    std::string outfilename = std::string(FilesVector[idFile].filename) + SUFFIX;
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
    if (fwrite(ptrHeader, 1, bytesRead, pOutfile) != bytesRead)
    {
      if (QUITE_MODE >= 1)
      {
        perror("fwrite");
        std::fprintf(stderr, "Failed writing to output file %s\n", outfilename.c_str());
      }
      return false;
    }
    //  Write of the workers compressed data
    for (size_t j = 0; j < numW; ++j)
    {
      if (activeWorkers[j] != -1)
      {
        if (fwrite((FilesVector[idFile].arrayOfPointers[j] + sizeOfT * (activeWorkers[j] + 1)), 1, compressedByWorkerSize[j], pOutfile) != compressedByWorkerSize[j])
        {
          if (QUITE_MODE >= 1)
          {
            perror("fwrite");
            std::fprintf(stderr, "Failed writing to output file %s\n", outfilename.c_str());
          }
          return false;
        }
      }
    }
    if (fclose(pOutfile) != 0)
      return false;
  }
  return true;
}

static inline bool mpiMasterDecompressing(size_t i, int numP)
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

  // Size of the uncompressed file
  size_t uncompressedFileSize;
  memcpy(&uncompressedFileSize, ptr, sizeof(size_t));

  // Number of blocks taken from header
  size_t numberOfBlocks;
  memcpy(&numberOfBlocks, ptr + sizeOfT, sizeof(size_t));

  size_t headerSize = sizeOfT * (numberOfBlocks + 2);
  size_t bytesRead = headerSize;

  size_t numberTasks = numberOfBlocks / numW;
  size_t overflowTasks = numberOfBlocks % numW;

  MPI_Request rq_send[numW];
  MPI_Status statuses[numW];
  int sentMessages = 0;
  // We skip the the uncompressed file size and the number of blocks from the header
  int tot = sizeOfT * 2;
  if (!workingMaster)
  {

    size_t bytesToSendForEachWorker[numW];
    size_t numberOfBlocksForEachWorker[numW];
    size_t displacement[numW + 1];
    size_t tempValue;
    for (int j = 0; j < numW; ++j)
    {
      bytesToSendForEachWorker[j] = 0;
      numberOfBlocksForEachWorker[j] = 0;
      displacement[j] = 0;
    }

    // THIS LOOP IS TO SEND THE LENGTH OF EACH COMPRESSED BLOCK TO THE WORKERS
    for (int j = 0; j < numW; ++j)
    {
      if (overflowTasks > 0)
      {
        MPI_Isend((ptr + tot), sizeOfT * (numberTasks + 1), MPI_UNSIGNED_CHAR, j + 1, idFile, MPI_COMM_WORLD, &rq_send[j]);

        for (int z = 0; z < numberTasks + 1; z++)
        {
          memcpy(&tempValue, ptr + tot + z * sizeOfT, sizeOfT);
          bytesToSendForEachWorker[j] += tempValue;
        }
        tot += sizeOfT * (numberTasks + 1);
        overflowTasks--;
        numberOfBlocksForEachWorker[j] += numberTasks + 1;
        displacement[j + 1] += displacement[j] + (numberTasks + 1) * BIGFILE_LOW_THRESHOLD;
        sentMessages++;
      }
      else
      {
        if (numberTasks > 0)
        {
          MPI_Isend((ptr + tot), sizeOfT * (numberTasks), MPI_UNSIGNED_CHAR, j + 1, idFile, MPI_COMM_WORLD, &rq_send[j]);
          for (int z = 0; z < numberTasks; z++)
          {
            memcpy(&tempValue, ptr + tot + z * sizeOfT, sizeOfT);
            bytesToSendForEachWorker[j] += tempValue;
          }
          tot += sizeOfT * (numberTasks);
          numberOfBlocksForEachWorker[j] += numberTasks;
          displacement[j + 1] = displacement[j] + (numberTasks)*BIGFILE_LOW_THRESHOLD;
          sentMessages++;
        }
      }
    }
    // send to everyworker the chunks of data
    for (int j = 0; j < numW; ++j)
    {
      MPI_Isend((ptr + tot), bytesToSendForEachWorker[j], MPI_UNSIGNED_CHAR, j + 1, idFile, MPI_COMM_WORLD, &rq_send[j]);
      tot += bytesToSendForEachWorker[j];
    }

    unsigned char *ptrFinal = new unsigned char[uncompressedFileSize + BIGFILE_LOW_THRESHOLD];
    size_t finalSizeOfFile = 0;
    for (int j = 0; j < sentMessages; ++j)
    {
      MPI_Request rq_recv;
      MPI_Status status;
      // Using the probe the master knows the offset in where put the uncompressed data in the vector
      MPI_Probe(MPI_ANY_SOURCE, idFile, MPI_COMM_WORLD, &status);

      int sourceReceived = status.MPI_SOURCE;
      int split = numberOfBlocksForEachWorker[sourceReceived - 1] * BIGFILE_LOW_THRESHOLD;
      MPI_Irecv(ptrFinal + displacement[sourceReceived - 1], split, MPI_UNSIGNED_CHAR, status.MPI_SOURCE, idFile, MPI_COMM_WORLD, &rq_recv);
      // std::cout << "BIP4" << "\n";
      MPI_Wait(&rq_recv, &status);
      int countElements;
      MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &countElements);
      finalSizeOfFile += countElements;
    }

    // Writing to the file from this point till the end of the function

    const std::string infilename(FilesVector[idFile].filename);
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

    success = writeFile(outfilename, ptrFinal, finalSizeOfFile);
    unmapFile(ptr, FilesVector[idFile].size);
    delete[] ptrFinal;
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

        // Using the probe to get the tag so we can get an estimation of the data to receive
        MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == INT_MAX)
          break;

        int mpitag = status.MPI_TAG;
        //  Get an estimate of the data to recive
        int estimation = FilesVector[mpitag].size / numW + BIGFILE_LOW_THRESHOLD * 2;
        unsigned char *ptrIN = new unsigned char[estimation];
        MPI_Irecv(ptrIN, estimation, MPI_UNSIGNED_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &rq_recv);
        MPI_Wait(&rq_recv, &status);
        int countElements;
        MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &countElements);

        size_t idFile = mpitag;

        size_t infile_size = countElements;
        size_t sizeOfT = sizeof(size_t);

        unsigned char *ptr = ptrIN;

        // The data is splitted in blocks and sent to the Right Workers of Fast Flow all2all
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
      MPI_Status status;
      MPI_Request rq_recv;
      size_t sizeOfT = sizeof(size_t);
      do
      {
        // Using the probe to get the tag so we can get an estimation of the data to receive
        MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == INT_MAX)
          break;

        int mpitag = status.MPI_TAG;
        // If it is the first message coming from the master for that specific file
        // The master will send us the number of compressed blocks with their specific length
        if (FilesVector[mpitag].numBlocks == -1)
        {
          //  Get an estimate of the data to recive
          int idFile = mpitag;
          int estimation = (FilesVector[idFile].size * 3 / BIGFILE_LOW_THRESHOLD + 1) / numW;
          estimation *= sizeOfT;
          unsigned char *ptrIN = new unsigned char[estimation];
          MPI_Irecv(ptrIN, estimation, MPI_UNSIGNED_CHAR, 0, idFile, MPI_COMM_WORLD, &rq_recv);
          MPI_Wait(&rq_recv, &status);
          int countElements;
          MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &countElements);

          FilesVector[idFile].numBlocks = countElements / sizeOfT;
          FilesVector[idFile].sizeOfBlocks = new size_t[FilesVector[idFile].numBlocks];
          for (int j = 0; j < FilesVector[idFile].numBlocks; ++j)
          {
            memcpy(&FilesVector[idFile].sizeOfBlocks[j], ptrIN + sizeOfT * j, sizeOfT);
            // Used to get the exact estimation in the else branch
            FilesVector[idFile].compressedLength += FilesVector[idFile].sizeOfBlocks[j];
          }
        }
        else
        {
          int idFile = mpitag;
          int estimation = FilesVector[idFile].compressedLength;
          unsigned char *ptrDe = new unsigned char[estimation];
          MPI_Irecv(ptrDe, estimation, MPI_UNSIGNED_CHAR, 0, idFile, MPI_COMM_WORLD, &rq_recv);
          MPI_Wait(&rq_recv, &status);
          FilesVector[idFile].pointer = new unsigned char[BIGFILE_LOW_THRESHOLD * FilesVector[idFile].numBlocks];
          size_t bytesRead = 0;

          // Send blocks to the Right Workers of all2all
          for (size_t j = 0; j < FilesVector[idFile].numBlocks; ++j)
          {
            Task_t *t = new Task_t();
            t->blockid = j;
            t->idFile = idFile;
            t->nblocks = FilesVector[idFile].numBlocks;
            t->ptr = ptrDe;
            t->ptrOut = FilesVector[idFile].pointer;
            t->uncompreFileSize = BIGFILE_LOW_THRESHOLD * FilesVector[idFile].numBlocks;
            t->size = FilesVector[idFile].compressedLength;
            t->readBytes = bytesRead;
            memcpy(&t->cmp_size, &FilesVector[idFile].sizeOfBlocks[j], sizeof(size_t));
            bytesRead = bytesRead + t->cmp_size;
            ff_send_out(t);
          }
        }

      } while (status.MPI_TAG != INT_MAX);
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
      in->cmp_size = cmp_len;
      ff_send_out(in);
    }
    return GO_ON;
  }
};

struct Gatherer : ff_minode_t<Task_t>
{
  Task_t *svc(Task_t *in)
  {
    if (compressing)
    {
      size_t idFile = in->idFile;
      // Add the compressed block of memory to the array of pointers
      FilesVector[idFile].arrayOfPointers[in->blockid] = in->ptrOut;
      FilesVector[idFile].sizeOfBlocks[in->blockid] = in->cmp_size;
      FilesVector[idFile].compressedLength += in->cmp_size;

      int val = vectorOfCounters[idFile]++;
      // When we have all blocks we send the data
      if (val >= in->nblocks - 1)
      {
        // WRITE TO MASTER
        size_t sizeOfT = sizeof(size_t);
        size_t numberOfBlocks = in->nblocks;
        unsigned char *ptrToSend = new unsigned char[(sizeOfT * (in->nblocks + 1) + FilesVector[idFile].compressedLength)];
        memcpy(ptrToSend, &numberOfBlocks, sizeOfT);

        memcpy((ptrToSend + sizeOfT), FilesVector[idFile].sizeOfBlocks, sizeOfT * in->nblocks);
        size_t tot = sizeOfT * (in->nblocks + 1);
        for (int i = 0; i < in->nblocks; ++i)
        {
          memcpy(ptrToSend + tot, FilesVector[idFile].arrayOfPointers[i], FilesVector[idFile].sizeOfBlocks[i]);
          tot += FilesVector[idFile].sizeOfBlocks[i];
        }
        MPI_Request rq_send;
        MPI_Isend(ptrToSend, tot, MPI_UNSIGNED_CHAR, 0, idFile, MPI_COMM_WORLD, &rq_send);
        FilesVector[idFile].pointer = ptrToSend;
        for (int i = 0; i < in->nblocks; ++i)
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
      FilesVector[idFile].uncompressedLength += in->cmp_size;

      int val = vectorOfCounters[idFile]++;
      // When we have all blocks we send the data

      if (val >= in->nblocks - 1)
      {
        MPI_Request rq_send;
        MPI_Status status;
        // Send BLOCK
        MPI_Isend(FilesVector[idFile].pointer, FilesVector[idFile].uncompressedLength, MPI_UNSIGNED_CHAR, 0, idFile, MPI_COMM_WORLD, &rq_send);
        MPI_Wait(&rq_send, &status);
      }
    }
    return GO_ON;
  }
};
static inline bool mpiWorker(int myId, int numP, int numberOfWorkers)
{
  // Each worker has a all to all inside
  unsigned long long array[MAX_FILES_IN_DIRECTORY];
  MPI_Status status;
  MPI_Status statusProbe;
  int numberOfFiles = 0;
  MPI_Request rq_recv;
  MPI_Irecv(array, MAX_FILES_IN_DIRECTORY * sizeof(MPI_UNSIGNED_LONG_LONG), MPI_UNSIGNED_LONG_LONG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &rq_recv);
  MPI_Wait(&rq_recv, &status);
  MPI_Get_count(&status, MPI_UNSIGNED_LONG_LONG, &numberOfFiles);

  for (int i = 0; i < numberOfFiles; ++i)
  {
    FilesVector.emplace_back("a", array[i]);
    vectorOfCounters.emplace_back(0);
  }

  int countElements;
  //Creation of All to All
  std::vector<ff_node *> LW;
  std::vector<ff_node *> RW;

  size_t Rw = numberOfWorkers;
  LW.push_back(new L_Worker(myId, numP));

  for (size_t i = 0; i < Rw; ++i)
    RW.push_back(new R_Worker);

  // Adding Lworkers and Rworkers to a2a
  ff_a2a a2a;
  a2a.add_firstset(LW);
  a2a.add_secondset(RW);
  ff_Pipe<> pipe(a2a, new Gatherer);
  if (pipe.run_and_wait_end() < 0)
  {
    error("running a2a\n");
    return -1;
  }

  // delete all the allocated memory
  for (int i = 0; i < FilesVector.size(); ++i)
  {
    delete FilesVector[i].pointer;
  }

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

  // Get the number of processes
  MPI_Comm_rank(MPI_COMM_WORLD, &myId);
  MPI_Comm_size(MPI_COMM_WORLD, &numP);

  if (workingMaster)
    numW = numP;
  else
    numW = numP - 1;
  if (argc < 4)
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

  double start_time = MPI_Wtime();
  const size_t Rw = std::stol(argv[3]);

  struct stat statbuf;
  bool dir = false;

  // Handle of the master
  if (myId == 0)
  {
    if (stat(argv[2], &statbuf) == -1)
    {
      perror("stat");
      fprintf(stderr, "Error: stat %s\n", argv[argc]);
      MPI_Abort(MPI_COMM_WORLD, -1);
      return -1;
    }

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
      {
        //  std::cout << "NOT EQUAL NOT EQUAL NOT EQUAL FILESVECTOR ARRAY TO SEND" << "\n";
      }
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
        if (compressing)
        {
          mpiMasterCompressing(i, numP);
        }
        else
        {

          mpiMasterDecompressing(i, numP);
        }
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
  }
  else // Handle the workers
  {
    mpiWorker(myId, numP, Rw);
  }
  if (!success)

  // END
  {
    printf("Exiting with (some) Error(s)\n");
    return -1;
  }
  if (!myId)
  {
    std::cout << "TIME MPI: " << (MPI_Wtime() - start_time) * 1000 << " milliseconds." << std::endl;
    // printf("Exiting with Success\n");
  }
  MPI_Finalize();
  return 0;
}