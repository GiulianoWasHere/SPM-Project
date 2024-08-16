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
#if !defined _WRITER_HPP
#define _WRITER_HPP

#include <miniz.h>
#include <string>
#include <unordered_map>
#include <ff/ff.hpp>
#include <utility.hpp>
#include <datatask.hpp>


struct Write: ff::ff_node_t<Task> {
    Task *svc(Task *task) {
	assert(task->nblocks > 1);

	std::string filename;
	if (comp) filename = task->filename;
	else {
	    size_t found=task->filename.rfind(".part");
	    assert(found != std::string::npos);
	    filename=task->filename.substr(0,found);
	}
	std::unordered_map<std::string,size_t>::iterator it=M.find(filename);
	if ( it == M.end()) {
	    M[filename]=1lu;
	} else {
	    it->second +=1;
	    if ( it->second == task->nblocks) {
		if (comp) {
		    unmapFile(task->ptr, task->size);
		    
		    // ---------- TODO: this part should be improved
		    std::string dirname{"."},filename{task->filename};
		    size_t found=filename.find_last_of("/");
		    if (found != std::string::npos) {
			dirname=filename.substr(0,found+1);
			filename=filename.substr(found+1);
		    }		
		    std::string cmd="cd "+ dirname+ " && tar cf " + filename + SUFFIX + " " + filename + ".part*"+ SUFFIX + " --remove-files";
		    int r=0;
		    if ((r=system(cmd.c_str())) != 0) {
			if (QUITE_MODE>=1) 
			    std::fprintf(stderr, "System command %s failed\n", cmd.c_str());
			success = false;
		    }		
		    // ----------
		    if ((r==0) && REMOVE_ORIGIN) {
			unlink((dirname+filename).c_str());
		    }
		} else { // decompression
		    std::string dirname,dest{"./"},fname;
		    size_t found=filename.find_last_of("/");
		    assert(found != std::string::npos);
		    dirname=task->filename.substr(0,found);
		    found=dirname.find_last_of("/");
		    if (found != std::string::npos) {
			dest=dirname.substr(0,found+1);
		    }
		    found=filename.find_last_of("/");
		    if (found != std::string::npos)
			fname=filename.substr(found+1);
		    else fname=filename;

		    // final destination file
		    dest+=fname;
		    
		    bool error=false;
		    FILE *pOutfile = fopen(dest.c_str(), "wb");
		    if (!pOutfile) {
			if (QUITE_MODE>=1) {
			    perror("fopen");
			    std::fprintf(stderr, "Error, cannot open file %s\n", filename.c_str());
			}
			error = true;
		    }
		    for(size_t i=1;!error && i<=task->nblocks;++i) {
			std::string src=filename+".part"+std::to_string(i);
			unsigned char *ptr = nullptr;
			size_t size=0;
			if (!mapFile(src.c_str(), size, ptr)) { error=true; break; }
			if (!error && fwrite(ptr, 1, size, pOutfile) != size) {
			    if (QUITE_MODE>=1) {
				perror("fwrite");
				std::fprintf(stderr, "Failed writing to output file %s\n", filename.c_str());
			    }
			    error=true;
			    break;
			}
			unmapFile(ptr,size);
			unlink(src.c_str());
		    }
		    if (error) success = false;
		    removeDir(dirname, true);
		    fclose(pOutfile);		    		    
		    if (!error && REMOVE_ORIGIN) {
			unlink((dest+".zip").c_str());
		    }
		}
		M.erase(filename);
	    }
	}
	delete [] task->ptrOut;
	delete task;	
	return GO_ON;
    }
    void svc_end() {
	// sanity check
	if (M.size() > 0) {
	    // TODO: here we should remove all .part files
	    
	    if (QUITE_MODE>=1) {
		std::fprintf(stderr, "Error: hash map not empty\n");
	    }
	    success=false;
	}

	if (!success) {
	    if (QUITE_MODE>=1)
		std::fprintf(stderr, "Writer: Exiting with (some) Error(s)\n");
	    return;
	}
    }
    bool success=true;
    std::unordered_map<std::string, size_t> M;
};


#endif // _WRITER_HPP
