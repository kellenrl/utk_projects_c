#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "dllist.h"
#include "jrb.h"
#include "jval.h"
#include "fields.h"

typedef struct myfile {
	int fn_sz;
	int mode;
	char *fn;
	char *fbuf;
	long ino;
	long fsize;
	long mtime;
	int isLink;
} Myfile;

void initMyfile(Myfile *file);
void processFile(Myfile *file, Dllist dirs, JRB inodes);

//comparison function for use with general JRB functions
int compare(Jval v1, Jval v2)
{
  if (v1.l < v2.l) return -1;
  if (v1.l > v2.l) return 1;
  return 0;
}

int main(int argc, char* argv[]) {
	int mode, fn_sz, nrd;
	long mtime, ino, fsize;
	JRB inodes;
	Myfile *file;
	Dllist files, tlist;
	long long totalBytes = 0;

	files = new_dllist();
	inodes = make_jrb();

	//while still reading in data from stdin, gets filename size
	while((nrd = read(STDIN_FILENO, &fn_sz, 4))) {
		totalBytes += nrd;

		//check if read was valid
		if(nrd != 4) {
			fprintf(stderr, "Bad tarc file at byte %lli.  Tried to read filename size, but only got %i bytes.\n", totalBytes, nrd);
			exit(1);
		}

		file = malloc(sizeof(Myfile));

		//initializes Myfile struct file
		initMyfile(file);

		//gets filename and checks if read was valid, allocates files fn memory
		file->fn = malloc(sizeof(char)*fn_sz+1);	
		nrd = read(STDIN_FILENO, file->fn, fn_sz);
		totalBytes += nrd;
		if(nrd != fn_sz) {
			fprintf(stderr, "Bad tarc file at byte %lli.  File name size is %i, but bytes read = %i.\n", totalBytes, fn_sz, nrd);
			exit(1);
		}

		//gets inode and checks if read was valid
		nrd = read(STDIN_FILENO, &ino, 8);
		totalBytes += nrd;
		if(nrd != 8) {
			fprintf(stderr, "Bad tarc file for %s.  Couldn't read inode\n", file->fn);
			exit(1);
		}

		file->fn_sz = fn_sz;
		file->ino = ino;

		//checks if inode seen before, if not adds to tree and processes
		if(jrb_find_gen(inodes, new_jval_l(ino), compare) == NULL) {
			jrb_insert_gen(inodes, new_jval_l(ino), new_jval_s(file->fn), compare);

			//gets mode and checks if read was valid
			nrd = read(STDIN_FILENO, &mode, 4);
			totalBytes += nrd;
			if(nrd != 4) {
				fprintf(stderr, "Bad tarc file for %s.  Couldn't read mode\n", file->fn);
				exit(1);
			}

			//gets mtime and checks if read was valid
			nrd = read(STDIN_FILENO, &mtime, 8);
			totalBytes += nrd;
			if(nrd != 8) {
				fprintf(stderr, "read %i bytes when should have read 8", nrd);
				exit(1);
			}
			file->mode = mode;
			file->mtime = mtime;

			//if not a dir, processes file things
			if(!S_ISDIR(mode)) {

				//gets file size and checks is read was valid
				nrd = read(STDIN_FILENO, &fsize, 8);
				totalBytes += nrd;
				if(nrd != 8) {
					fprintf(stderr, "Bad tarc file for %s.  Couldn't read size\n", file->fn);
					exit(1);
				}

				//allocates memory in file struct for file buffer
				file->fbuf = malloc(sizeof(char)*fsize); 

				//gets file contents and puts into file struct fbuf, checks if read was valid
				nrd = read(STDIN_FILENO, file->fbuf, fsize);
				totalBytes += nrd;
				if(nrd != fsize) {
					fprintf(stderr, "Bad tarc file for %s.  Trying to read %li bytes of the file, and got EOF\n", file->fn, fsize);
					exit(1);
				}
				file->fsize = fsize;
			}
		}

		//otherwise this is a hardlink
		else {
			file->isLink = 1;
		}

		//puts file struct into files dllist for later processing
		dll_append(files, new_jval_v(file));

	}

	//dllist to hold directory file structs for later processing
	Dllist dirs = new_dllist();

	//goes through file dllist and process each file
	Myfile *mfp;
	dll_traverse(tlist, files) {
		mfp = (Myfile *)tlist->val.v;
		processFile(mfp, dirs, inodes);
	}

	//goes through dirs dllist and processes directories mode and mod times
	struct timeval times[2];
	dll_traverse(tlist, dirs) {
		mfp = (Myfile *)tlist->val.v;
		chmod(mfp->fn, mfp->mode);
		times[0].tv_sec = time(NULL);
		times[0].tv_usec = 0;
		times[1].tv_sec = mfp->mtime;
		times[1].tv_usec = 0;
		utimes(mfp->fn, times);
	}

	//memory cleanup
	dll_traverse(tlist, files) {
		mfp = (Myfile *)tlist->val.v;
		free(mfp->fn);
		free(mfp->fbuf);
		free(mfp);
	}

	free_dllist(dirs);
	free_dllist(files);
	jrb_free_tree(inodes);
    return 0;
}

//name - processFile
//brief - take a file struct pointer and processes data according to .tarc rules
//param[in] - Myfile *file - pointer to Myfile struct to br processed
//param[in] - Dllist dirs - dllist to hold dirs for later processing
//param[in] - Dllist inodes - used to find inode and its filename for making hard links

void
processFile(Myfile *file, Dllist dirs, JRB inodes)
{
	FILE *fp;
	struct timeval times[2];
	JRB rnode;

	//if is a directory, process and add to dllist dirs to set mode later
	if(S_ISDIR(file->mode)) {
		mkdir(file->fn, 1777);
		dll_append(dirs, new_jval_v(file));

	}	

	//else it is a file, process
	else {

		//if is a hard link, get inodes filename and make link
		if(file->isLink == 1) {
			rnode = jrb_find_gen(inodes, new_jval_l(file->ino), compare);
			link(rnode->val.s, file->fn);
		}

		//else process file things
		else {
			fp = fopen(file->fn, "w");
			fwrite(file->fbuf, file->fsize, 1, fp);
			fclose(fp);
			chmod(file->fn, file->mode);
			times[0].tv_sec = time(NULL);
			times[0].tv_usec = 0;
			times[1].tv_sec = file->mtime;
			times[1].tv_usec = 0;
			utimes(file->fn, times);
		}
	}
}

//name - initMyfile
//brief - initializes a Myfile struct's data
//param[in] - Myfile *file - file to be initialized
//param[out] - Myfile *file - data no initialized

void
initMyfile(Myfile *file)
{
	file->fn_sz = 0;
	file->mode = 0;
	file->fn = NULL;
	file->fbuf = NULL;
	file->ino = 0;
	file->fsize = 0;
	file->mtime = 0;
	file->isLink =0;
}
