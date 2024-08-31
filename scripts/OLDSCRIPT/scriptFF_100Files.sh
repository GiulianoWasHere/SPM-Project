#!/bin/bash
#SBATCH --job-name=FF_100FILES     # Job name
#SBATCH --output=JOB_FF_%j.txt     # OUTPUT
#SBATCH --nodes=1                  # Number of nodes
#SBATCH --ntasks-per-node=1        # Number of tasks
#SBATCH --time=01:00:00            # Time

GENERATE_TXT=../generateTxt
EXE_PATH=../FF_minizip
DELETE_SCRIPT=./deleteMiniz.sh
CHECK_FILES=./checkFilze.sh
#Files Info
FILES_PATH=../LeFilze/
NAME_OF_FILE=10MB
DIMENSION_FILE=10
NUMBER_OF_FILES=11
alphabet=({A..Z})
numLetters=26
echo NUMBER OF FILES: $NUMBER_OF_FILES EACH: $DIMENSION_FILE MB

#Generate Files
for i in $(seq $NUMBER_OF_FILES)
do
    FILENAME=$i"_"${NAME_OF_FILE}
    $GENERATE_TXT $DIMENSION_FILE $FILES_PATH$FILENAME
done

for l in 1 2 3; do
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

#Delete Files
for i in $(seq $NUMBER_OF_FILES)
do
    FILENAME=$i"_"${NAME_OF_FILE}
    find $FILES_PATH -name $FILENAME -type f -delete
done
