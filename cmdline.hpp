/*
 * Simple file compressor/decompressor using Miniz and the FastFlow 
 * building blocks
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
#if !defined _CMDLINE_HPP
#define _CMDLINE_HPP

#include <cstdio>
#include <string>

#include <utility.hpp>

// some global variables. A few others are in utility.hpp -----------------------------------
static long nworkers=0;  // the number of farm Workers
static bool cc=false;    // concurrency control, default is blocking
static bool aN=1;        // parameter of the set_scheduling_ondemand (i.e. channel's capacity
// ------------------------------------------------------------------------------------------

static inline void usage(const char *argv0) {
    std::printf("--------------------\n");
    std::printf("Usage: %s [options] file-or-directory [file-or-directory]\n",argv0);
    std::printf("\nOptions:\n");
    std::printf(" -n set the n. of Workers (default nworkers=%ld)\n", ff_numCores());
    std::printf(" -t set the \"BIG file\" low threshold (in Mbyte -- min. and default %ld Mbyte)\n",BIGFILE_LOW_THRESHOLD/(1024*1024) );
    std::printf(" -r 0 does not recur, 1 will process the content of all subdirectories (default r=0)\n");
    std::printf(" -C compress: 0 preserves, 1 removes the original file (default C=0)\n");
    std::printf(" -D decompress: 0 preserves, 1 removes the original file\n");
    std::printf(" -q 0 silent mode, 1 prints only error messages to stderr, 2 verbose (default q=1)\n");
    std::printf(" -a asynchrony degree for the on-demand policy (default a=1)\n");
    std::printf(" -b 0 blocking, 1 non-blocking concurrency control (default b=0)\n");
    std::printf("--------------------\n");
}

int parseCommandLine(int argc, char *argv[]) {
    extern char *optarg;
    const std::string optstr="n:t:r:C:D:q:a:b:";
    
    nworkers = ff_numCores();
    long opt, aN = 1, start = 1;
    bool cpresent=false, dpresent=false;
    while((opt = getopt(argc, argv, optstr.c_str())) != -1) {
	switch(opt) {
	case 'n': {
	    if (!isNumber(optarg, nworkers)) {
		std::fprintf(stderr, "Error: wrong '-n' option\n");
		usage(argv[0]);
		return -1;
	    }
	    start+=2;
	} break;
	case 't': {
	    long t=0;
	    if (!isNumber(optarg, t)) {
		std::fprintf(stderr, "Error: wrong '-t' option\n");
		usage(argv[0]);
		return -1;
	    }
	    t = std::min(2l, t);
	    BIGFILE_LOW_THRESHOLD= t*(1024*1024);
	    start+=2;
	    if (BIGFILE_LOW_THRESHOLD > 100*1024*1024) { // just to set a limit
		std::fprintf(stderr, "Error: \"BIG file\" low threshold too high %ld\n", BIGFILE_LOW_THRESHOLD);
		return -1;
	    }
	} break;
	case 'r': {
	    long n=0;
	    if (!isNumber(optarg, n)) {
		std::fprintf(stderr, "Error: wrong '-r' option\n");
		usage(argv[0]);
		return -1;
	    }
	    if (n == 1) RECUR = true;
	    start +=2;	    
	} break;
	case 'C': {
	    long c=0;
	    if (!isNumber(optarg, c)) {
			std::fprintf(stderr, "Error: wrong '-C' option\n");
			usage(argv[0]);
			return -1;
	    }
	    cpresent=true;
	    if (c == 1) REMOVE_ORIGIN = true;
	    start+=2; 
	} break;
	case 'D': {
	    long d=0;
	    if (!isNumber(optarg, d)) {
		std::fprintf(stderr, "Error: wrong '-D' option\n");
		usage(argv[0]);
		return -1;
	    }	    
	    dpresent=true;
	    if (d == 1) REMOVE_ORIGIN = true;
	    comp = false;
	    start+=2;
	} break;
	case 'q': {
	    long q=0;
	    if (!isNumber(optarg, q)) {
		std::fprintf(stderr, "Error: wrong '-D' option\n");
		usage(argv[0]);
		return -1;
	    }	    	    
	    QUITE_MODE=q;
	    start+=2; 
	} break;
	case 'a': {
	    if (!isNumber(optarg, aN)) {
		std::fprintf(stderr, "Error: wrong '-a' option\n");
		usage(argv[0]);
		return -1;
	    }	    	    
	    aN = std::min(1l, aN);
	    start+=2;
	} break;
	case 'b': {
	    long b=0;
	    if (!isNumber(optarg, b)) {
		std::fprintf(stderr, "Error: wrong '-b' option\n");
		usage(argv[0]);
		return -1;
	    }	    	    
	    if (b == 1) cc = false;
	    start +=2;
	} break;
	default:
	    usage(argv[0]);
	    return -1;
	}
    }
    if (cpresent && dpresent) {
		std::fprintf(stderr, "Error: -C and -D are mutually exclusive!\n");
		usage(argv[0]);
		return -1;
    }
    if ((argc-start)<=0) {
		std::fprintf(stderr, "Error: at least one file or directory should be provided!\n");
		usage(argv[0]);
		return -1;
    }
    return start;
}

#endif // _CMDLINE_HPP
