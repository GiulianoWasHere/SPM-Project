#!/bin/bash
#
# Author: Massimo Torquati
#
# File compression script leveraging Miniz comp/decomp capabilities.
# This script shows how the compression of "BIG files" is done.
#
# It splits "BIG file" in multiple independent files (the splitting
# size is defined by THSIZE), then compress all of them with Miniz
# and finally merge the compressed parts using the tar command in a
# single file with SUFFIX name extension.
#

#set -x -v

SUFFIX=".zip"

# splitting threshold
THSIZE=2M
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
    echo "Error, file \'$infile\' does not exist!"
    exit -1;   
fi
# get the extension, if any
ext=$([[ "$infile" = *.* ]] && echo "${infile##*.}" || echo '')
if [ .$ext != ."" ] && [ .$ext == $SUFFIX ]; then
    echo "Error, $infile has already a $SUFFIX extension -- ignored"
    exit -1
fi

# check if the file is a "BIG file"
sb=$(find "$infile" -size +$THSIZE)
if [ -z $sb ]; then # not a "BIG file", simple case
    r=$($CD c "$infile" >& /dev/null)
    if [ $? -ne 0 ]; then
	echo "Error: cannot compress the file"
	exit -1
    fi
else
    # the file must be split in multiple parts
    tmpdir=$(mktemp -d)
    if [ $? -ne 0 ]; then  
	echo "Error: cannot create a temporary directory"
	exit -1
    fi
    path=${infile%/*}    # this is the path
    filename=${infile##*/}
    cp $infile $tmpdir
    if [ $? -ne 0 ]; then  
	echo "Error: copy the input file into a temporary directory"
	exit -1
    fi
    split -d -b $THSIZE $tmpdir/$filename $tmpdir/$filename >& /dev/null
    if [ $? -ne 0 ]; then  
	echo "Error: splitting the input file in parts"
	exit -1
    fi
    rm $tmpdir/$filename

    # compressing each part, then removing the original files
    r=$($CD C $tmpdir >& /dev/null)
    if [ $? -ne 0 ]; then
	echo "Error: cannot compress the files"
	exit -1
    fi
    pushd $tmpdir >& /dev/null
    # create a TAR file containing all parts
    tar cf $filename$SUFFIX *$SUFFIX >& /dev/null
    if [ $? -ne 0 ]; then  
	echo "Error: error during tar"
	exit -1
    fi
    popd >& /dev/null
    mv $tmpdir/$filename$SUFFIX .
    if [ $? -ne 0 ]; then  
	echo "Error: cannot move the ompressed file in $PWD"
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
