#!/bin/bash
#SBATCH --job-name=FF_2GB         # Job name
#SBATCH --output=JOB_FF_%j.txt    # OUTPUT
#SBATCH --nodes=1                  # Number of nodes
#SBATCH --time=01:00:00            # Time

GENERATE_TXT=../generateTxt
EXE_PATH=../FF_minizip
DELETE_SCRIPT=./deleteMiniz.sh
CHECK_FILES=./checkFilze.sh
#Files Info
ScratchDir="/tmp/myjob"  # create a name for the TEMP directory (The TXT file will be in the temp directory of the NODE)
mkdir -p ${ScratchDir}   # make the directory
ScratchDir="/tmp/myjob/"
NAME_OF_FILE=2GB
DIMENSION_FILE=2048

echo FILE: $NAME_OF_FILE DIMENSION: $DIMENSION_FILE MB
$GENERATE_TXT $DIMENSION_FILE $ScratchDir$NAME_OF_FILE

for l in 1 2 3 4 5 6 7 8; do
    for r in 4 8 16 20 22 24 26 28 29 30 31 32; do
        echo "LWORKERS: $l RWORKERS: $r"
        echo "COMPRESSION:"
        $EXE_PATH c $ScratchDir $l $r
        echo "DECOMPRESSION:"
        $EXE_PATH d $ScratchDir $l $r
        #Check if uncompressed and the original file are equal
        $CHECK_FILES $ScratchDir
        $DELETE_SCRIPT $ScratchDir
        echo "-----------------------"
    done
done
find $ScratchDir -name $NAME_OF_FILE -type f -delete