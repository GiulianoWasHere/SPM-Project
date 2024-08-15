/*
 * Simple file compressor/decompressor using Miniz and the FastFlow 
 * building blocks. It is an example showing some features of the 
 * FastFlow parallel library.
 * 
 * Miniz source code: https://github.com/richgel999/miniz
 * https://code.google.com/archive/p/miniz/
 * 
 * FastFlow: https://github.com/fastflow/fastflow
 *
 * Author: Massimo Torquati <massimo.torquati@unipi.it>
 *                                                              
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the author be held liable for any damages
 * arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose.
 *
 */
/* Comments and version history:
 * This code is a mix of POSIX C code and some C++ library calls.
 *
 *  0.1     May 2020   First version
 *                     It uses recursion for walking the directories 
 *                     (if '-r' is set), therefore it is prone to the
 *                     stack overflow problem.
 *
 *
 */
/*
 * Parallel schema:
 *
 *  |<----------- FastFlow's farm ---------->| 
 *
 *               |---> Worker --->|
 *               |                |
 *   Reader ---> |---> Worker --->| --> Writer
 *               |                |
 *               |---> Worker --->|
 *
 *  "small files", those whose size is < BIGFILE_LOW_THRESHOLD
 *  "BIG files", all other files
 *
 * ------------
 * Compression: 
 * ------------
 * "small files" are memory-mapped by the Reader while they are
 * compressed and written into the FS by the Workers.
 *
 * "BIG files" are split into multiple independent files, each one 
 * having size less than or equal to BIGFILE_LOW_THRESHOLD, then
 * all of them will be compressed in by the Workers. Finally, all 
 * compressed files owning to the same "BIG file" are merged 
 * into a single file by using the 'tar' command. 
 * Reader: memory-map the input file, and splits it into multiple parts
 * Worker: compresses the assigned parts and then sends them to the Writer
 * Writer: waits for the compression of all parts and combine all of them 
 * together in a single file tar-file.
 *
 * --------------
 * Decompression:
 * --------------
 * The distinction between small and BIG files is done by checking
 * the header (magic number) of the file.
 *
 * "small files" are directly forwarded to the Workers that will 
 * do all the work (reading, decompressing, writing).
 *
 * "BIG files" are untarred into a temporary directory and then 
 * each part is sent to the Workers. The generic Worker decompresses
 * the assigned parts and then sends them to the Writer. 
 * The Writer waits for to receive all parts and then merges them
 * in the result file.
 *
 */


#include <cstdio>

#include <ff/ff.hpp>
#include <ff/pipeline.hpp>

#include <datatask.hpp>
#include <utility.hpp>
#include <cmdline.hpp>
#include <reader.hpp>
#include <worker.hpp>
#include <writer.hpp>


int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }
    // parse command line arguments and set some global variables
    long start=parseCommandLine(argc, argv);
    if (start<0) return -1;

    // ----- defining the FastFlow network ----------------------
    Read reader(const_cast<const char**>(&argv[start]), argc-start);
    Write writer;
    std::vector<ff::ff_node*> Workers;
    for(long i=0;i<nworkers;++i) 
		Workers.push_back(new Worker);
    ff::ff_farm farm(Workers,reader,writer);
    farm.cleanup_workers(); 
    farm.set_scheduling_ondemand(aN);
    farm.blocking_mode(cc);
	farm.no_mapping(); 
    //NOTE: mapping here!!!
    // ----------------------------------------------------------
    
    if (farm.run_and_wait_end()<0) {
        if (QUITE_MODE>=1) ff::error("running farm\n");
        return -1;
    }
    
    bool success = true;
    success &= reader.success;
    success &= writer.success;
    for(size_t i=0;i<Workers.size(); ++i)
	success &= reinterpret_cast<Worker*>(Workers[i])->success;
    if (success) {
	if (QUITE_MODE>=1) printf("Done.\n");
	return 0;
    }

    return -1; 
}

