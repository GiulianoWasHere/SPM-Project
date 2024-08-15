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
#if !defined _WORKER_HPP
#define _WORKER_HPP

#include <miniz.h>
#include <string>
#include <ff/ff.hpp>
#include <datatask.hpp>
#include <utility.hpp>

struct Worker: ff::ff_node_t<Task> {
    Task *svc(Task *task) {
		bool oneblockfile = (task->nblocks == 1);
		if (comp) {
			unsigned char * inPtr = task->ptr;	
			size_t          inSize= task->size;
			
			// get an estimation of the maximum compression size
			unsigned long cmp_len = compressBound(inSize);
			// allocate memory to store compressed data in memory
			unsigned char *ptrOut = new unsigned char[cmp_len];
			if (compress(ptrOut, &cmp_len, (const unsigned char *)inPtr, inSize) != Z_OK) {
				if (QUITE_MODE>=1) std::fprintf(stderr, "Failed to compress file in memory\n");
				success = false;
				delete [] ptrOut;
				delete task;
				return GO_ON;
			}
			task->ptrOut   = ptrOut;
			task->cmp_size = cmp_len;  // real length
			std::string outfile{task->filename};
			if (!oneblockfile) {
				outfile += ".part"+std::to_string(task->blockid);
			} 
			outfile += SUFFIX;
			
			// write the compressed data into disk 
			bool s = writeFile(outfile, task->ptrOut, task->cmp_size);
			if (s && REMOVE_ORIGIN && oneblockfile) {
				unlink(task->filename.c_str());
			}
			if (oneblockfile) {
				unmapFile(task->ptr, task->size);	
				delete [] task->ptrOut;
				delete task;
				return GO_ON;
			}
			
			if (s) ff_send_out(task);  // sending to the Writer
			else {
				if (QUITE_MODE>=1)
					std::fprintf(stderr, "Error writing file %s\n", task->filename.c_str());
				success = false;
				delete [] task->ptrOut;
				delete task;	    
			}	    
			return GO_ON;
		}
		// decompression part
		bool remove = !oneblockfile || REMOVE_ORIGIN;
		if (decompressFile(task->filename.c_str(), task->size, remove) == -1) {
			if (QUITE_MODE>=1) 
				std::fprintf(stderr, "Error decompressing file %s\n", task->filename.c_str());
			success=false;
		}
		if (oneblockfile) {
			delete task;
			return GO_ON;
		}
		return task; // sending to the Writer
    }
    void svc_end() {
		if (!success) {
			if (QUITE_MODE>=1) std::fprintf(stderr, "Worker %ld: Exiting with (some) Error(s)\n", get_my_id());
			return;
		}
    }
    bool success = true;
};

#endif // _WORKER_HPP
