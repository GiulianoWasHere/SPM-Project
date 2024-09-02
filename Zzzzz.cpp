int shuffleID(int z, int numberOfWorkers, int id)
{
    return ((z + id) % numberOfWorkers)+1;
}

int inverseShuffleIDForZ(int numberOfWorkers, int id, int result)
{
    return (result - 1 - id + numberOfWorkers) % numberOfWorkers;
}
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

int main()
{
    int idFile = 0;
    int numberOfWorkers = 8;
    int z = 0;
    int output = shuffleID(z,numberOfWorkers,idFile);
    std::cout << output << "\n";
    std::cout << inverseShuffleIDForZ(numberOfWorkers,idFile,output) << "\n";
    return 0;
}