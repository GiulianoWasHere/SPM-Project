#!/bin/bash
#
# Author: Massimo Torquati
#
# File decompression script leveraging Miniz comp/decomp capabilities.
# This script shows how the decompression of "BIG files" is done.
#
# If the original file was a "BIG file" (i.e. it was split in multiple
# independent files), it opens the file in a temporary directory (using tar),
# decompress each part by using CD and then merge them all in asingle
# monolithic file.
# Instead, if the file was not a "BIG file", it just calls CD to decompress it.
#

#set -x -v

SUFFIX=".zip"

# where the compressor/decompressor executable should be
CD=/usr/local/bin/compdecomp

if [ ! -z ${COMPDECOMP} ]; then
    CD=${COMPDECOMP}
fi
# sanity checks
if [ ! -f  $CD ]; then
    echo "Error cannot find the 'compdecomp' executable"
    if [ -z ${COMPDECOMP} ]; then
	echo "Please set the COMPDECOMP env variable with the executable path"
    else
	echo "${COMPDECOMP} is not valid"
    fi
    exit -1
fi
if [ ! -x  $CD ]; then
    echo "Error, " $CD " is not executable"
    exit -1
fi

# checking input
if [ $# -ne 1 ]; then    
    echo use: $(basename $0) file
    exit -1
fi
infile=$1               
if [ ! -f "$infile" ]; then
    echo "Error, file '$infile' does not exist!"
    exit -1;   
fi
ext=${infile##*.}
if [ .$ext != $SUFFIX ]; then
    echo "Error, '$infile' has an invalid extension, it should be $SUFFIX"
    exit -1
fi

# check if the file was a "BIG file" compressed in multiple parts
# "BIG compressed files" have a tar magic header 
r=$(file "$infile" | grep tar)
if [ $? -ne 0 ]; then  # not a "BIG file", simple case
    r=$($CD d "$infile" >& /dev/null)
    if [ $? -ne 0 ]; then
	echo "Error: cannot decompress the file"
	tmpfile="$infile"
	tmpfile+="_decomp"
	rm -f $tmpfile >& /dev/null
	exit -1
    fi
else
    # the file is a TAR composed by multiple compressed parts
    tmpdir=$(mktemp -d)
    if [ $? -ne 0 ]; then  
	echo "Error: cannot create a temporary directory"
	exit -1
    fi
    # opening the tar file
    tar xf "$infile" -C $tmpdir
    if [ $? -ne 0 ]; then  
	echo "Error: error during untar"
	exit -1
    fi
    # decompressing the single parts, then removing the compressed file
    $CD D $tmpdir >& /dev/null
    if [ $? -ne 0 ]; then  
	echo "Error: error decompressing file parts"
	exit -1
    fi
    # how many parts?
    nblocks=$(find $tmpdir -type f | wc -l)
    
    f1=${infile%.*} # removing the extension  
    f2=${f1##*/}    # this is the filename
    
    # merging the uncompressed parts
    for i in $(seq 1 1 $nblocks); do
	find $tmpdir -type f -name "*.part$i" -exec cat {} >> $tmpdir/$f2 \;
	if [ $? -ne 0 ]; then  
	    echo "Error: error merging file parts"
	    exit -1
	fi    
    done
    finaldir=$(dirname $infile)
    mv $tmpdir/$f2 $finaldir
    if [ $? -ne 0 ]; then  
	echo "Error: cannot move the uncompressed file in $PWD"
	exit -1
    fi    
    rm -fr $tmpdir >& /dev/null
fi
echo "Done."
echo -n "Remove the '$infile' file (S/N)? "
read yn
if [ x$yn == x"S" ]; then
    rm -f "$infile"
fi

exit 0
