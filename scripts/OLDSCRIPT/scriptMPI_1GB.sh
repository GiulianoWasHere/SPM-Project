#!/bin/bash
#SBATCH --job-name=MPI_1GB         # Job name
#SBATCH --output=JOB_MPI_%j.txt    # OUTPUT
#SBATCH --nodes=8                  # Number of nodes
#SBATCH --ntasks-per-node=1        # Number of tasks
#SBATCH --time=01:00:00            # Time

#Executables info
GENERATE_TXT=../generateTxt
EXE_PATH=../MPI_minizip
DELETE_SCRIPT=./deleteMiniz.sh
CHECK_FILES=./checkFilze.sh

#Files Info
FILES_PATH=../LeFilze/
NAME_OF_FILE=1GB
DIMENSION_FILE=1024

echo FILE: $NAME_OF_FILE DIMENSION: $DIMENSION_FILE MB
$GENERATE_TXT $DIMENSION_FILE $FILES_PATH$NAME_OF_FILE
for l in 3 4 5 6 7 8; do
    for r in 4 8 16 20 22 24 26 28 29 30 31 32; do
        echo "NODES: $l RWORKERS: $r"
        echo "COMPRESSION:"
        mpirun -np $l --map-by ppr:1:node $EXE_PATH c $FILES_PATH $r
        echo "DECOMPRESSION:"
        mpirun -np $l --map-by ppr:1:node $EXE_PATH d $FILES_PATH $r
        #Check if uncompressed and the original file are equal
        $CHECK_FILES $FILES_PATH
        $DELETE_SCRIPT $FILES_PATH
        echo "-----------------------"
    done
done

find $FILES_PATH -name $NAME_OF_FILE -type f -delete