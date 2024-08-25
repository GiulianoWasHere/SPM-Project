#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>

static inline void usage(const char *argv0)
{
  printf("--------------------\n");
  printf("Usage: %s SizeOfFileInMB NameOfFile \n", argv0);
  printf("--------------------\n");
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        usage(argv[0]);
        return -1;
    }

    size_t numberOfKB = std::stol(argv[1]) * 1024 * 1024;
    
    std::srand(static_cast<unsigned int>(std::time(0)));
    std::string fileOut(argv[2]);
    std::ofstream outFile(fileOut);

    if (!outFile)
    {
        std::cerr << "Error opening file!" << std::endl;
        return -1;
    }

    for (int i = 0; i < numberOfKB; ++i)
    {
        char randomLetter = 'a' + std::rand() % 26; 
        outFile << randomLetter;
    }

    // Close the file
    outFile.close();

    return 0;
}
