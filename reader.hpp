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
#if !defined _READER_HPP
#define _READER_HPP

#include <ff/ff.hpp>
#include <datatask.hpp>

// reader node, this is the "Emitter" of the FastFlow farm 
struct Read: ff::ff_node_t<Task> {
    Read(const char **argv, int argc): argv(argv), argc(argc) {}

    // ------------------- utility functions 
    bool doWorkCompress(const std::string& fname, size_t size) {
		unsigned char *ptr = nullptr;
		if (!mapFile(fname.c_str(), size, ptr)) return false;
		if (size<= BIGFILE_LOW_THRESHOLD) {
			Task *t = new Task(ptr, size, fname);
			ff_send_out(t); // sending to the next stage
		} else {
			const size_t fullblocks  = size / BIGFILE_LOW_THRESHOLD;
			const size_t partialblock= size % BIGFILE_LOW_THRESHOLD;
			for(size_t i=0;i<fullblocks;++i) {
				Task *t = new Task(ptr+(i*BIGFILE_LOW_THRESHOLD), BIGFILE_LOW_THRESHOLD, fname);
				t->blockid=i+1;
				t->nblocks=fullblocks+(partialblock>0);
				ff_send_out(t); // sending to the next stage
			}
			if (partialblock) {
				Task *t = new Task(ptr+(fullblocks*BIGFILE_LOW_THRESHOLD), partialblock, fname);
				t->blockid=fullblocks+1;
				t->nblocks=fullblocks+1;
				ff_send_out(t); // sending to the next stage
			}
		}
		return true;
    }
    bool doWorkDecompress(const std::string& fname, size_t size) {
		int r = checkHeader(fname.c_str());
		if (r<0) { // fatal error in checking the header
	    if (QUITE_MODE>=1) 
			std::fprintf(stderr, "Error: checking header for %s\n", fname.c_str());
	    return false;
	}
	if (r>0) { // it was a small file compressed in the standard way
	    ff_send_out(new Task(nullptr, size, fname)); // sending to one Worker
	    return true;
	}
	// fname is a tar file (maybe), is was a "BIG file"       	
	std::string tmpdir;
	size_t found=fname.find_last_of("/");
	if (found != std::string::npos)
	     tmpdir=fname.substr(0,found+1);
	else tmpdir="./";

	// this dir will be removed in the Writer
	if (!createTmpDir(tmpdir))  return false;
	// ---------- TODO: this part should be improved
	std::string cmd="tar xf " + fname + " -C" + tmpdir;
	if ((r=system(cmd.c_str())) != 0) {
	    std::fprintf(stderr, "System command %s failed\n", cmd.c_str());
	    removeDir(tmpdir, true);
	    return false;
	}
	// ----------
	DIR *dir;
	if ((dir=opendir(tmpdir.c_str())) == NULL) {
	    if (QUITE_MODE>=1) {
			perror("opendir");
			std::fprintf(stderr, "Error: opening temporary dir %s\n", tmpdir.c_str());
	    }
	    removeDir(tmpdir, true);
	    return false;
	}
	std::vector<std::string> dirV;
	dirV.reserve(200); // reserving some space
	struct dirent *file;
	bool error=false;
	while((errno=0, file =readdir(dir)) != NULL) {
	    std::string filename = tmpdir + "/" + file->d_name;
	    if ( !isdot(filename.c_str()) ) dirV.push_back(filename);
	}
	if (errno != 0) {
	    if (QUITE_MODE>=1) perror("readdir");
	    error=true;
	}
	closedir(dir);
	size_t nfiles=dirV.size();
	for(size_t i=0;i<nfiles; ++i) {
	    Task *t = new Task(nullptr, 0, dirV[i]);
	    t->blockid=i+1;
	    t->nblocks=nfiles;
	    ff_send_out(t); // sending to the next stage	    
	}	
	return !error;
    }
    
    bool doWork(const std::string& fname, size_t size) {
		if (comp)  // compression
			return doWorkCompress(fname, size);	
		return doWorkDecompress(fname, size);
    }
    
    bool walkDir(const std::string &dname) {
		DIR *dir;	
		if ((dir=opendir(dname.c_str())) == NULL) {
			if (QUITE_MODE>=1) {
				perror("opendir");
				std::fprintf(stderr, "Error: opendir %s\n", dname.c_str());
			}
			return false;
		}
		struct dirent *file;
		bool error=false;
		while((errno=0, file =readdir(dir)) != NULL) {
			if (comp && discardIt(file->d_name, true)) {
				if (QUITE_MODE>=2)
					std::fprintf(stderr, "%s has already a %s suffix -- ignored\n", file->d_name, SUFFIX);		
				continue;
			}
			struct stat statbuf;
			std::string filename = dname + "/" + file->d_name;
			if (stat(filename.c_str(), &statbuf)==-1) {
				if (QUITE_MODE>=1) {
					perror("stat");
					std::fprintf(stderr, "Error: stat %s (dname=%s)\n", filename.c_str(), dname.c_str());
				}
				continue;
			}
			if(S_ISDIR(statbuf.st_mode)) {
				assert(RECUR);
				if ( !isdot(filename.c_str()) ) {
					if (!walkDir(filename)) error = true;
				}
			} else {
				if (!comp && discardIt(filename.c_str(), false)) {
					if (QUITE_MODE>=2)
						std::fprintf(stderr, "%s does not have a %s suffix -- ignored\n", filename.c_str(), SUFFIX);
					continue;
				}
				if (statbuf.st_size==0) {
					if (QUITE_MODE>=2)
						std::fprintf(stderr, "%s has size 0 -- ignored\n", filename.c_str());
					continue;		    
				}
				if (!doWork(filename, statbuf.st_size)) error = true;
			}
		}
		if (errno != 0) {
			if (QUITE_MODE>=1) perror("readdir");
			error=true;
		}
		closedir(dir);
		return !error;
    }    
    // ------------------- fastflow svc and svc_end methods 
	
    Task *svc(Task *) {
		for(long i=0; i<argc; ++i) {
			if (comp && discardIt(argv[i], true)) {
				if (QUITE_MODE>=2) 
					std::fprintf(stderr, "%s has already a %s suffix -- ignored\n", argv[i], SUFFIX);
				continue;
			}
			struct stat statbuf;
			if (stat(argv[i], &statbuf)==-1) {
				if (QUITE_MODE>=1) {
					perror("stat");
					std::fprintf(stderr, "Error: stat %s\n", argv[i]);
				}
				continue;
			}
			if (S_ISDIR(statbuf.st_mode)) {
				if (!RECUR) continue;
				success &= walkDir(argv[i]);
			} else {
				if (!comp && discardIt(argv[i], false)) {
					if (QUITE_MODE>=2)
						std::fprintf(stderr, "%s does not have a %s suffix -- ignored\n", argv[i], SUFFIX);
					continue;
				}
				if (statbuf.st_size==0) {
					if (QUITE_MODE>=2)
						std::fprintf(stderr, "%s has size 0 -- ignored\n", argv[i]);
					continue;		    
				}
				success &= doWork(argv[i], statbuf.st_size);
			}
		}
        return EOS; // computation completed
    }
    
    void svc_end() {
		if (!success) {
			if (QUITE_MODE>=1) 
				std::fprintf(stderr, "Read stage: Exiting with (some) Error(s)\n");		
			return;
		}
    }
    // ------------------------------------
    const char **argv;
    const int    argc;
    bool success = true;
};

#endif // _READER_HPP
