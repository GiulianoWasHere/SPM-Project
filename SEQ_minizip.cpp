#include <utility.hpp>

static inline void usage(const char *argv0)
{
    printf("--------------------\n");
    printf("Usage: %s c|d|C|D file-or-directory\n", argv0);
    printf("\nModes:\n");
    printf("c - Compresses file infile to a zlib stream into outfile\n");
    printf("d - Decompress a zlib stream from infile into outfile\n");
    printf("--------------------\n");
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        usage(argv[0]);
        return -1;
    }
    const char *pMode = argv[1];
    --argc;
    if (!strchr("cCdD", pMode[0]))
    {
        printf("Invalid option!\n\n");
        usage(argv[0]);
        return -1;
    }
    const bool compress = ((pMode[0] == 'c') || (pMode[0] == 'C'));

    //TIMER
    const auto start = std::chrono::steady_clock::now();

    bool success = true;
    struct stat statbuf;
    if (stat(argv[argc], &statbuf) == -1)
    {
        perror("stat");
        fprintf(stderr, "Error: stat %s\n", argv[argc]);
        return -1;
    }
    bool dir = false;
    if (S_ISDIR(statbuf.st_mode))
    {
        success &= walkDir(argv[argc], compress);
    }
    else
    {
        success &= doWork(argv[argc], statbuf.st_size, compress);
    }

    if (!success)
    {
        printf("Exiting with (some) Error(s)\n");
        return -1;
    }
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    std::cout << "Time Sequential: " << duration.count() << " milliseconds" << std::endl;
    return 0;
}
