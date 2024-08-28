#!/bin/bash
#SBATCH --job-name=MPI_1GB         # Job name
#SBATCH --output=JOB_MPI_%j.txt    # OUTPUT
#SBATCH --nodes=1                  # Number of nodes
#SBATCH --ntasks-per-node=1        # Number of tasks
#SBATCH --time=01:00:00            # Time

#Executables 
GENERATE_TXT=../generateTxt
EXE_PATH=../FF_minizip
DELETE_SCRIPT=./deleteMiniz.sh
CHECK_FILES=./checkFilze.sh
#Files Info
FILES_PATH=../LeFilze/
NAME_OF_FILE=200MB
DIMENSION_FILE=200

echo FILE: $NAME_OF_FILE DIMENSION: $DIMENSION_FILE MB
$GENERATE_TXT $DIMENSION_FILE $FILES_PATH$NAME_OF_FILE
for l in 1; do
    for r in 4 8 16 20 22 24 26 28 29 30 31 32; do
        echo "LWORKERS: $l RWORKERS: $r"
        echo "COMPRESSION:"
        $EXE_PATH c $FILES_PATH $l $r
        echo "DECOMPRESSION:"
        $EXE_PATH d $FILES_PATH $l $r
        #Check if uncompressed and the original file are equal
        #$CHECK_FILES $FILES_PATH
        $DELETE_SCRIPT $FILES_PATH
        echo "-----------------------"
    done
done
find $FILES_PATH -name $NAME_OF_FILE -type f -delete