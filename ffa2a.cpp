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

#include <cmath>
#include <string>
#include <vector>
#include <iostream>
#include <ff/ff.hpp>
#include <ff/all2all.hpp>
using namespace ff;
#include <utility.hpp>
// ------------ GLOBAL VARIBLES ---------------
size_t vectorSize = 0;
// ------------ END GLOBAL VARIBLES ---------------

struct FileStruct
{
  std::string filename;
  size_t size;
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
/* struct Task_t {
    Task_t(unsigned char *ptr, size_t size, const std::string &name):
        ptr(ptr),size(size),filename(name) {}

    unsigned char    *ptr;           // input pointer
    size_t            size;          // input size
    unsigned char    *ptrOut=nullptr;// output pointer
    size_t            cmp_size=0;    // output size
    size_t            blockid=1;     // block identifier (for "BIG files")
    size_t            nblocks=1;     // #blocks in which a "BIG file" is split
    const std::string filename;      // source file name
}; */

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
  const std::string filename;      // source file name
};
struct L_Worker : ff_monode_t<Task_t>
{ // must be multi-output

  L_Worker(std::vector<FileStruct> &FilesVector, size_t id, size_t numberOfTasks, size_t NumberOfLWorkers)
      : FilesVector(FilesVector), id(id), numberOfTasks(numberOfTasks), NumberOfLWorkers(NumberOfLWorkers) {}

  Task_t *svc(Task_t *)
  {
    // Based on the Id (The id is given at the creation of the node)
    // the files are divided for each worker
    for (size_t i = 0; i < numberOfTasks; ++i)
    {
      const std::string infilename(FilesVector[id + i * NumberOfLWorkers].filename);
      size_t infile_size = FilesVector[id + i * NumberOfLWorkers].size;
      size_t sizeOfT = sizeof(size_t);

      unsigned char *ptr = nullptr;
      if (!mapFile(infilename.c_str(), infile_size, ptr))
      {
        success = false;
        continue;
      }

      const size_t fullblocks = infile_size / BIGFILE_LOW_THRESHOLD;
      const size_t partialblock = infile_size % BIGFILE_LOW_THRESHOLD;
      size_t numberOfBlocks = fullblocks;
      // Estimate of the length of the compressed file
      unsigned long compressedFileLength = mz_compressBound(BIGFILE_LOW_THRESHOLD * fullblocks + partialblock);
      // ff_send_out(new Task_t(FilesVector[id + i*NumberOfLWorkers].filename));

      if (partialblock)
        numberOfBlocks++;

      // add the header size
      size_t tot = 0;

      //Mandare i blocchi ad ogni reader
      //Creando oggetto task
      //serve la size del blocco 
      //Id del file 
      //AGGIUNGERE nel FileVECTOR un numero per contare i numeri di blocchi Arrivati, magari anche un vettore per tenere i puntatori dei blocchi compressi
      for (size_t i = 0; i < fullblocks; ++i)
      {
      }
      if (partialblock)
      {
      }
    }
    return EOS;
  }
  std::vector<FileStruct> FilesVector;
  size_t id;
  size_t numberOfTasks;
  size_t NumberOfLWorkers;
  bool success = true;
};
struct R_Worker : ff_minode_t<Task_t>
{ // must be multi-input
  R_Worker(const size_t Lw) : Lw(Lw) {}
  Task_t *svc(Task_t *in)
  {
    std::cout << in->filename << "\n";
    // SendToWriter
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
  const bool compressing = ((pMode[0] == 'c') || (pMode[0] == 'C'));
  REMOVE_ORIGIN = ((pMode[0] == 'C') || (pMode[0] == 'D'));

  const size_t Lw = std::stol(argv[3]);
  const size_t Rw = std::stol(argv[4]);

  std::cout << Rw << "\n";
  std::cout << argv[2] << "\n";

  bool success = true;
  struct stat statbuf;
  if (stat(argv[2], &statbuf) == -1)
  {
    perror("stat");
    fprintf(stderr, "Error: stat %s\n", argv[argc]);
    return -1;
  }
  bool dir = false;
  std::vector<FileStruct> FilesVector;

  // Walks in the directory and add the filenames in the FileVector
  if (S_ISDIR(statbuf.st_mode))
  {
    success &= walkDirff(argv[2], compressing, FilesVector);
  }
  else
  {
    success &= addFileToVector(argv[2], statbuf.st_size, compressing, FilesVector);
  }

  /* vectorSize = FilesVector.size();
  for (FileStruct i : FilesVector)
    std::cout << i.filename << i.size << "\n";

  std::cout << FilesVector[0].filename << "\n";
  std::cout << vectorSize << "\n"; */

  std::vector<ff_node *> LW;
  std::vector<ff_node *> RW;
  size_t numberTasks = FilesVector.size() / Lw;
  size_t overflowTasks = FilesVector.size() % Lw;
  for (size_t i = 0; i < Lw; ++i)
  {
    if (overflowTasks)
    {
      LW.push_back(new L_Worker(FilesVector, i, numberTasks + 1, Lw));
      overflowTasks--;
    }
    else
      LW.push_back(new L_Worker(FilesVector, i, numberTasks, Lw));
  }
  for (size_t i = 0; i < Rw; ++i)
    RW.push_back(new R_Worker(Lw));

  // Adding Lworkers and Rworkers to a2a
  ff_a2a a2a;
  a2a.add_firstset(LW);
  a2a.add_secondset(RW);

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