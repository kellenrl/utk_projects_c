#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "dllist.h"
#include "jrb.h"
#include "jval.h"
#include "fields.h"

void splitDirName(char* dir, char *prefix, char *suffix);
void processDir(char *fn, JRB inodes);

//comparison function for using general type JRB functions
int compare(Jval v1, Jval v2)
{
  if (v1.l < v2.l) return -1;
  if (v1.l > v2.l) return 1;
  return 0;
}

int 
main(int argc, char* argv[])
{
	JRB inodes;
	int exists;
	struct stat buf;

	inodes = make_jrb();

	//error msg for invalid args
	if(argc != 2) {
		fprintf(stderr, "usage: tarc directory\n");
		return 1;
	}

	//stat argv[1], error is doesnt exist or is not a directory
	exists = lstat(argv[1], &buf);
	if(exists < 0) {
		fprintf(stderr, "Couldn't stat %s\n", argv[1]);
		exit(1);
	}
	if(!S_ISDIR(buf.st_mode)) {
		fprintf(stderr, "%s is not a directory\n", argv[1]);
		exit(1);
	}

	processDir(argv[1], inodes);

	jrb_free_tree(inodes);
}

//name - processDir
//brief - processes a directory and its files and sub dirs, prints .tarc info to stdout
//param[in] - char *fn - the directory/file name
//param[in] - JRB inodes - used to store inodes and check if seen or not

void
processDir(char *fn, JRB inodes)
{
	char *dir_fn, *suf, *pre;
	DIR *d;
	struct dirent *de;
	struct stat buf;
	int exists, fn_size, dir_fn_size, sz;
	Dllist directories, tlist;
	static int count = 0;
	static int prefixLength = 0;

	d = opendir(fn);
	if(d == NULL) {
		perror(fn);
		exit(1);
	}
	
	directories = new_dllist();

	//allocate memory and make the path name
	fn_size = strlen(fn);
	dir_fn_size = fn_size + 10;
	dir_fn = (char *) malloc(sizeof(char) * dir_fn_size);
	if(dir_fn == NULL) { perror("malloc dir_fn"); exit(1); }
	strcpy(dir_fn, fn);
	strcat(dir_fn + fn_size, "/");

	//allocate memory for storage of prefix and suffix
	pre = malloc(sizeof(char)*MAXNAMLEN);
	suf = malloc(sizeof(char)*MAXNAMLEN);
	
	//splits prefix and suffix of pathname into separate char*
	splitDirName(fn, pre, suf);

	//if this is the first run of the function, sets prefix length for use in printing
	if(count == 0) {
		prefixLength = (int)strlen(pre);
	}

	//stats file/dir and print error if doesnt exist
	exists = lstat(fn, &buf);
	if(exists < 0) {
		fprintf(stderr, "Couldn't stat %s\n", dir_fn);
		exit(1);
	}

	//check if we have seen this inode, if not add to JRB inodes and process
	if(jrb_find_gen(inodes, new_jval_l(buf.st_ino), compare) == NULL) {
		if(count == 0) {
		sz = strlen(suf);
		fwrite(&sz, sizeof(int), 1, stdout);
		fwrite(suf, strlen(suf), 1, stdout);
		fwrite(&buf.st_ino, sizeof(long), 1, stdout);
		}
		else { 
		sz = strlen(dir_fn);
		fwrite(&sz, sizeof(int), 1, stdout);
		fwrite(dir_fn+prefixLength, strlen(dir_fn), 1, stdout);
		fwrite(&buf.st_ino, sizeof(long), 1, stdout);
		}
	}
	
	if(jrb_find_gen(inodes, new_jval_l(buf.st_ino), compare) == NULL) {
		jrb_insert_gen(inodes, new_jval_l(buf.st_ino), new_jval_i(0), compare);
		fwrite(&buf.st_mode, sizeof(int), 1, stdout);
		fwrite(&buf.st_mtime, sizeof(long), 1, stdout);
	}

	//loops through all current dirs files and sub dirs and process
	for(de = readdir(d); de != NULL; de = readdir(d)) {

		if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
		
		sz = strlen(de->d_name);
		if(dir_fn_size < fn_size + sz + 2) { 
			dir_fn_size = fn_size + sz + 10;
			dir_fn = realloc(dir_fn, dir_fn_size);
		}
		strcpy(dir_fn + fn_size + 1, de->d_name);

		exists = lstat(dir_fn, &buf);
		if(exists < 0) {
			fprintf(stderr, "Couldn't stat %s\n", dir_fn);
			exit(1);
		}

		//these are processed for every file
		sz = strlen(dir_fn);
		fwrite((&sz), sizeof(int), 1, stdout);
		fwrite(dir_fn+prefixLength, strlen(dir_fn), 1, stdout);
		fwrite(&buf.st_ino, sizeof(long), 1, stdout);
	
		//these are processed only if not seen the inode before
		if(jrb_find_gen(inodes, new_jval_l(buf.st_ino), compare) == NULL) {
			jrb_insert_gen(inodes, new_jval_l(buf.st_ino), new_jval_i(0), compare);
			fwrite(&buf.st_mode, sizeof(int), 1, stdout);
			fwrite(&buf.st_mtime, sizeof(long), 1, stdout);
		}

		//if we have seen it before, continue with next iteration of loop
		else{
			continue;
		}

		//if this is a dir, process dir things
		if(S_ISDIR(buf.st_mode)) {
			dll_append(directories, new_jval_s(strdup(dir_fn)));
		}
		//else it is a file, process file things
		else {
			fwrite(&buf.st_size, sizeof(long), 1, stdout);

			FILE *fp = fopen(dir_fn, "rb");
			fseek(fp, 0, SEEK_END);
			long long fsize = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			
			char *fbuf = malloc(fsize + 1);
			fread(fbuf, 1, fsize, fp);
			fclose(fp);
			fbuf[fsize] = 0;

			fwrite(fbuf, fsize, 1, stdout);

			free(fbuf);
		}
	}
	count++;
	closedir(d);

	//recursively process all dirs in directories dllist
	dll_traverse(tlist, directories) {
		processDir(tlist->val.s, inodes);
	}

	//memory cleanup
	dll_traverse(tlist, directories) free(tlist->val.s);
	free_dllist(directories);
	free(pre);
	free(suf);
	free(dir_fn);
}

//name - splitDirName
//brief - takes file name and splits its prefix and suffix
//param[in] - char *dir - a directory filename
//param[in] - char *prefix - mem already allocated
//param[in] - char *suffic - mem already allocated
//param[out] - char *prefix - this now holds the dirs prefix
//param[out] - char *suffic - this now holds the dirs suffix

void
splitDirName(char *dir, char *prefix, char *suffix)
{
	int i;
	for(i = strlen(dir); i >= 0; i--) {
		if(dir[i] == '/') {
			strncpy(prefix, dir, i+1);
			strcpy(suffix, dir+(i+1));
			return;
		}
	} 
	strcpy(suffix, dir);
	strcpy(prefix, dir);
	return;
}
