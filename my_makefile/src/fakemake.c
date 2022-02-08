#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "dllist.h"
#include "jval.h"
#include "fields.h"
#include "jrb.h"

const int NOT_FOUND = 0;
const int OPEN_ERROR = -1;
const int READ_ERROR = -2;
const int MAX_LINE_LENGTH = 2048;

time_t processHfiles(Dllist);
time_t processCfiles(Dllist, time_t, int*, Dllist, Dllist);
time_t fileFind(char*);
void compileCfiles(Dllist, Dllist);
void buildExe(Dllist, Dllist, Dllist, char*);

int 
main (int argc, char *argv[])
{
	IS is;
	char *fname, *exe;
	Dllist ffiles, lfiles, hfiles, cfiles, tlist, addfiles; 
	int nfiles = 0;
	int nlines = 0;
	time_t omax, hmax, exetime;
	
	exe = NULL;
	fname = NULL;
	cfiles = new_dllist();
	hfiles = new_dllist();
	lfiles = new_dllist();
	ffiles = new_dllist();
	addfiles = new_dllist();

	//error checking and getting discription file name
	if(argc != 1 && argc != 2) {
		fprintf(stderr, "usage: ./fakemake [ decription file ]\n");
		return 1;
	}
	if(argc == 1) {
		fname = strdup("fmakefile");
	}
	else {
		fname = strdup(argv[1]);
	}

	//open file via input_struct
	is = new_inputstruct(fname);
	if(is == NULL) {
		fprintf(stderr, "Error opening %s\n", fname);
		perror(fname);
		return 1;
	}
	
	//while file is not empty
	while(get_line(is) >= 0) {
		nlines++;

		//if line is blank, continue
		if(is->NF == 0) continue;

		//line not blank, process
		else {

			//if line starts with C
			if(is->text1[0] == 'C'){
				int i;
				for(i = 1; i < is->NF; i++) {
					//check if file name ends with .c
					if(is->fields[i][strlen(is->fields[i])-2] != '.' &&
							is->fields[i][strlen(is->fields[i])-1] != 'c') {
						fprintf(stderr, "./fakemake: C file %s needs to end with .c\n", is->fields[i]);
						return 1;
					}
					dll_append(cfiles, new_jval_s(strdup(is->fields[i])));
				}
			} 

			//if line starts with H 
			else if(is->text1[0] == 'H'){
				int i;
				for(i = 1; i < is->NF; i++) {
					//check if file name ends with .h
					if(is->fields[i][strlen(is->fields[i])-2] != '.' &&
							is->fields[i][strlen(is->fields[i])-1] != 'h') {
						fprintf(stderr, "./fakemake: H file %s needs to end with .h\n", is->fields[i]);
						return 1;
					}
					dll_append(hfiles, new_jval_s(strdup(is->fields[i])));
				}
			} 

			//if line starts with L 
			else if(is->text1[0] == 'L'){
				int i;
				for(i = 1; i < is->NF; i++) {
					dll_append(lfiles, new_jval_s(strdup(is->fields[i])));
				}
			} 

			//if line starts with F 
			else if(is->text1[0] == 'F'){
				int i;
				for(i = 1; i < is->NF; i++) {
					dll_append(ffiles, new_jval_s(strdup(is->fields[i])));
				}
			} 

			//if line starts with E
			else if(is->text1[0] == 'E'){
				if(exe != NULL) {
					fprintf(stderr, "fmakefile (%i) cannot have more than one E line\n", nlines);
					return 1;
				}
				exe = strdup(is->fields[1]);
			} 

			//if line starts with incorrect line type flag char
			else {
				fprintf(stderr, "Error, unprocessed line starting with %s\n", is->fields[0]);
				return 1;
			}
		}
	}

	if(exe == NULL) {
		fprintf(stderr, "No executable specified\n");
		return 1;
	}
	
	//process hfiles
	hmax = processHfiles(hfiles);	
	if(hmax < 0) return 1;

	//process cfiles
	omax = processCfiles(cfiles, hmax, &nfiles, addfiles, ffiles); //ERROR HERE, this is not omax this is cmax	
	if(omax < 0) {
		//compileCfiles(addfiles, ffiles);
		return 1;
	}

	//if any cfiles were updated, recompile them
	if(!dll_empty(addfiles)) compileCfiles(addfiles, ffiles);
	
	//find exe and return it's mtime
	exetime = fileFind(exe);

	//if exe does not need to be remade
	if(exetime > 0 && exetime > omax && (dll_empty(addfiles))) printf("%s up to date\n", exe);
	else {
		buildExe(ffiles, cfiles, lfiles, exe);
	}

	//deallocate cfile jvals
	dll_traverse(tlist, cfiles) {
		free(tlist->val.s);
	}	

	//deallocate hfile jvals
	dll_traverse(tlist, hfiles) {
		free(tlist->val.s);
	}	

	//deallocate lfile jvals
	dll_traverse(tlist, lfiles) {
		free(tlist->val.s);
	}	

	//deallocate ffile jvals
	dll_traverse(tlist, ffiles) {
		free(tlist->val.s);
	}	

	free_dllist(cfiles);
	free_dllist(hfiles);
	free_dllist(lfiles);
	free_dllist(ffiles);
	free_dllist(addfiles);

	free(exe);
	free(fname);

	jettison_inputstruct(is);

	return 0;
}

//name - processHfiles
//brief - checks if h files exists and returns max mtime
//param[in] - Dllist hfiles, list of header file names
//return - the max mtime of the header files as a time_t

time_t
processHfiles(Dllist hfiles)
{
	int exists;
	time_t max = 0;
	struct stat st;
	Dllist tlist;

	dll_traverse(tlist, hfiles) {
		exists = stat(tlist->val.s, &st);
		if(exists < 0) {
			fprintf(stderr, "File %s does not exist\n", tlist->val.s);
			return((time_t)READ_ERROR);
		}
		else {
			if(st.st_mtime > max) max = st.st_mtime;
		}
	}
	return(max);
}

//name - processCfiles
//brief - goes through cfiles dllist, checks if they need to be recompiled. if any are
//        recompiled returns 0 and nfiles is # of files remade. if not returns max mtime
//        of the found .o files.
//param1[in] - Dllist of cfile names
//param2[in] - time_t max mtime of header files
//param3[out] - nfiles is now number of remade .c files
//param4[in] - Dllist of remade c files
//param5[in] - Dllist of flag names
//return - max mtime of .o files

time_t
processCfiles(Dllist cfiles, time_t hmax, int *nfiles, Dllist addfiles, Dllist ffiles)
{
	int exists;
	struct stat st;
	Dllist tlist;
	time_t otime, omax;

	//number of files that were remade
	nfiles = 0;
	
	//max mtime of .o files, 0 if none found
	omax = 0;

	dll_traverse(tlist, cfiles) {
		//error if file doesnt exist
		exists = stat(tlist->val.s, &st);
		if(exists < 0) {
			compileCfiles(addfiles, ffiles);
			fprintf(stderr, "fmakefile: ");
			perror( tlist->val.s);
			return((time_t)READ_ERROR);
		}
		//if file exists, look for corresponding .o file
		else {
			//get cfile.o string
			char *oname;
			oname = strndup(tlist->val.s, (strlen(tlist->val.s)-2));
			strcat(oname, ".o");

			//search directory for cfile.o
			otime = fileFind(oname);
			if(otime > omax) omax = otime;
			if(otime == 0 || otime < st.st_mtime || otime < hmax) {
				dll_append(addfiles, new_jval_s(tlist->val.s));
				nfiles++;
			}
			free(oname);
		}
	}
	return(omax); 
}

//searches current directory for file, if found returns its st_mtime
//name - fileFind
//brief - searches current directory for file, if found return it's mtime
//param[in] - char * name of file to look for
//return - time_t of found files mtime, 0 if not found, negative if errors

time_t
fileFind(char *name)
{
	time_t mtime;
	DIR *dirp;
	struct dirent *dp;
	struct stat st;

	dirp = opendir(".");

	while (dirp) {
		errno = 0;
	    if ((dp = readdir(dirp)) != NULL) {
			if (strcmp(dp->d_name, name) == 0) {
				closedir(dirp);
				stat(name, &st);
				mtime = st.st_mtime;
				return(mtime);
			}
		} else {
			if (errno == 0) {
				closedir(dirp);
				return((time_t)NOT_FOUND);
			}
        closedir(dirp);
        return((time_t)READ_ERROR);
		}
	}
	return((time_t)OPEN_ERROR);
}

//name - compileCfiles
//brief - creates gcc -c compile string with files in dllists given and runs it via system()
//param1[in] - list of files to be recompiled
//param2[in] - list of flags to be used during compilation

void
compileCfiles(Dllist addfiles, Dllist ffiles) 
{
	Dllist tlist;
	char gccStr[MAX_LINE_LENGTH];
	char gccStr2[MAX_LINE_LENGTH];
	strcpy(gccStr, "gcc -c");

	dll_traverse(tlist, ffiles) {
		strcat(gccStr, " ");
		strcat(gccStr, tlist->val.s);
	}
	strcpy(gccStr2, gccStr);

	dll_traverse(tlist, addfiles) {
		strcat(gccStr2, " ");
		strcat(gccStr2, tlist->val.s);
		printf("%s\n", gccStr2);
		if(system(gccStr2) != 0) {
			fprintf(stderr, "Command failed.  Exiting\n");
			exit(1);
		}
		strcpy(gccStr2, gccStr);
	}
}

//name - buildExe
//brief - creates gcc -o string with files in given dllists and runs it via system()
//param1[in] - list of flags
//param2[in] - list of .o file names
//param3[in] - list of library includes
//param4[in] - name of executable being created

void
buildExe(Dllist ffiles, Dllist cfiles, Dllist lfiles, char *exe)
{
	Dllist tlist;
	char gccStr[MAX_LINE_LENGTH];
	char gccStr2[MAX_LINE_LENGTH];
	strcpy(gccStr, "gcc");

	strcat(gccStr, " -o ");
	strcat(gccStr, exe);

	dll_traverse(tlist, ffiles) {
		strcat(gccStr, " ");
		strcat(gccStr, tlist->val.s);
	}

	strcpy(gccStr2, gccStr);

	dll_traverse(tlist, cfiles) {
		strcat(gccStr2, " ");
		strcat(gccStr2, tlist->val.s);
		gccStr2[strlen(gccStr2)-1] = 'o';
	}

	dll_traverse(tlist, lfiles) {
		strcat(gccStr2, " ");
		strcat(gccStr2, tlist->val.s);
	}

	printf("%s\n", gccStr2);
	if(system(gccStr2) != 0) {
		fprintf(stderr, "Command failed.  Fakemake exiting\n");
		exit(1);
		}
}
