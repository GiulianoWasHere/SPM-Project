#!/bin/bash
#SBATCH --job-name=SEQ_100Files         # Job name
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
NAME_OF_FILE=20MB
DIMENSION_FILE=20
NUMBER_OF_FILES=100
echo NUMBER OF FILES: $NUMBER_OF_FILES EACH: $DIMENSION_FILE MB

#Generate Files
for i in $(seq $NUMBER_OF_FILES)
do
    FILENAME=$i"_"${NAME_OF_FILE}
    $GENERATE_TXT $DIMENSION_FILE $ScratchDir$FILENAME
done

echo "COMPRESSION:"
$EXE_PATH c $ScratchDir
echo "DECOMPRESSION:"
$EXE_PATH d $ScratchDir
$CHECK_FILES $ScratchDir
$DELETE_SCRIPT $ScratchDir
echo "-----------------------"

#Delete Files
for i in $(seq $NUMBER_OF_FILES)
do
    FILENAME=$i"_"${NAME_OF_FILE}
    find $ScratchDir -name $FILENAME -type f -delete
done