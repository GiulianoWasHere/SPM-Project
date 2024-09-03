#!/bin/bash
#SBATCH --job-name=SEQ_2GB         # Job name
#SBATCH --output=JOB_SEQ_%j.txt    # OUTPUT
#SBATCH --nodes=1                  # Number of nodes
#SBATCH --time=01:00:00            # Time

GENERATE_TXT=../generateTxt
EXE_PATH=../SEQ_minizip
DELETE_SCRIPT=./deleteMiniz.sh
CHECK_FILES=./checkFilze.sh
#Files Info
ScratchDir="/tmp/myjob"  # create a name for the TEMP directory (The TXT file will be in the temp directory of the NODE)
rm -rf ${ScratchDir}     # Making sure the folder hasn't file in it
mkdir -p ${ScratchDir}   # make the directory
ScratchDir="/tmp/myjob/"
NAME_OF_FILE=2GB
DIMENSION_FILE=2048

echo FILE: $NAME_OF_FILE DIMENSION: $DIMENSION_FILE MB
$GENERATE_TXT $DIMENSION_FILE $ScratchDir$NAME_OF_FILE
echo "COMPRESSION:"
$EXE_PATH c $ScratchDir
echo "DECOMPRESSION:"
$EXE_PATH d $ScratchDir
$CHECK_FILES $ScratchDir
$DELETE_SCRIPT $ScratchDir
echo "-----------------------"
find $ScratchDir -name $NAME_OF_FILE -type f -delete