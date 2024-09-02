#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

int main()
{
    // Input and output file names
    std::string inputFile = "/home/giuliano/Desktop/SPM-Project/TestFolder/MPI_Tests/NEW/MPI_3MB_2GB_TEMPFOLDER";
    // std::string outputFile = "output1.csv";
    std::string outputName = "MPI_3MB_2GB_TEMPFOLDER_COMP";

    std::ifstream infile(inputFile);
    std::ofstream outfile;

    // Check if files are opened successfully
    if (!infile.is_open())
    {
        std::cerr << "Could not open input file.\n";
        return 1;
    }

    // Writing the header of the CSV file
    // std::string csvHeader = "nodes,RWORKERS,Compression Time (ms),Decompression Time (ms),File Status\n";
    std::string csvHeader = "r,ct\n";
    // outfile << csvHeader;

    int minTime = 9999;
    std::string line;
    int firstFile = true;
    int oldlworker = 1;
    int nodes = 1, rworkers = 0, compressionTime = 0, decompressionTime = 0;
    std::string fileStatus;

    // Loop to process the input file
    while (std::getline(infile, line))
    {
        if (line.find("NODES") != std::string::npos)
        {
            // Extracting nodes and RWORKERS
            std::istringstream ss(line);
            std::string ignore;
            ss >> ignore >> nodes >> ignore >> rworkers;
            if (firstFile)
            {
                outfile.open(outputName + std::to_string(nodes) + ".csv");
                if (!outfile.is_open())
                {
                    std::cerr << "Could not open output file.\n";
                    return 1;
                }
                oldlworker = nodes;
                outfile << csvHeader;
                firstFile = false;
            }
            else if (nodes != oldlworker)
            {
                outfile.close();
                outfile.open(outputName + std::to_string(nodes) + ".csv");
                if (!outfile.is_open())
                {
                    std::cerr << "Could not open output file.\n";
                    return 1;
                }
                oldlworker = nodes;
                outfile << csvHeader;
            }
        }
        else if (line.find("TIME MPI:") != std::string::npos)
        {
            // Extracting Time (either Compression or Decompression)
            std::istringstream ss(line);
            std::string ignore;
            int time;
            ss >> ignore >> ignore >> time >> ignore;

            // Determine whether this time is for compression or decompression
            if (compressionTime == 0)
            {
                compressionTime = time;
            }
            else
            {
                decompressionTime = time;
            }
        }
        // else if (line.find("is Identical!") != std::string::npos)
        else if (line.find("-----------------------") != std::string::npos)
        {
            // Extracting File Status
            fileStatus = "Identical";

            // Writing the extracted information to the CSV file
            /* outfile << nodes << "," << rworkers << ","
                    << compressionTime << "," << decompressionTime << ","
                    << fileStatus << "\n"; */

            /*outfile << nodes << "," << rworkers << ","
                    << compressionTime << "\n";*/

            //COMPRESSION TIME
            outfile << rworkers << ","
                    << (static_cast< float >(compressionTime)/1000) << "\n";

            if(minTime > compressionTime)
            {
                std::cout << "WORKERS : " <<rworkers << "Nodes: " << nodes << "TIME: " << compressionTime << "\n";
                minTime = compressionTime;
            }

            //DECOMPRESSION TIME
           
            /* outfile << rworkers << ","
                    << (static_cast< float >(decompressionTime)/1000) << "\n"; */
            
            //SPEEDUP

            //COMPRESSION TIME
            /* outfile << rworkers+2 << ","
                    << (static_cast< float >(75882)/static_cast< float >(compressionTime)) << "\n"; */

            //DECOMPRESSION TIME
           
            /* outfile << rworkers+2 << ","
                    << (static_cast< float >(18478)/static_cast< float >(decompressionTime)) << "\n"; */

            // Reset times for the next set of workers

            //EFFICENCY 

             //COMPRESSION TIME
            /* outfile << rworkers+1 << ","
                    << (static_cast< float >(75882)/(static_cast< float >(compressionTime) * rworkers+1)) << "\n"; */

            /* outfile << rworkers+1 << ","
                    << (static_cast< float >(18478)/(static_cast< float >(decompressionTime) * rworkers+1)) << "\n"; */
            
            /* outfile << compressionTime << "\n"; */


            //WEAK SCALING

            /* outfile << rworkers + nodes << ","
                    << compressionTime << "\n"; */

            /* outfile 
                    << compressionTime * (rworkers + nodes) << "\n"; */
            compressionTime = 0;
            decompressionTime = 0;
        }
    }

    // Close the file streams
    infile.close();
    outfile.close();

    //std::cout << "Transformation complete. Data written to " << "\n";

    return 0;
}
