/* filename: l2p2.c
 * author: Kellen Leland
 *
 * This is the main src file for lab2 part 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "fields.h"
#include "jval.h"
#include "dllist.h"
#include "jrb.h"

#define MAX_NAME_LEN 253

typedef struct ip {
	unsigned char address[4];
	Dllist names;
} IP;

int main (int argc, char *argv[])
{
	int fin; //use with open()
	unsigned char *fbuf; //file buffer
	char *name, *tokname, *lname;
	char **nameArr;
	IP *ip, *ip2;
	int pos, numNames, arrCnt, nrd;
	JRB hosts, tmp, tmp2;
	Dllist tlist, tlist2, tlist3;

	fin = open("converted", O_RDONLY);
	
	//if invalid arguments given, print error msg and exit
	if(argc != 1) {
		fprintf(stderr, "Usage: ./l2p1\n");
		return 1;
	}

	//print error and exit if error opening file
	if(fin == -1) {
		fprintf(stderr, "Error opening input file 'converted'.");
		return 1;
	}

	//read in the entire file into a buffer and close the file
	long long fsize = lseek(fin, 0, SEEK_END);
	lseek(fin, 0, SEEK_SET);

	fbuf = malloc(fsize + 1);
	nrd = read(fin, fbuf, fsize);
	fbuf[fsize] = '\0';

	close(fin);

	//variable to track position in the file buffer array
	pos = 0;

	//create JRB tree for hosts, allocate memory to store names
	hosts = make_jrb();
	arrCnt = 0;
	nameArr = malloc(sizeof(char*)*1);

	//while there is still data in the buffer, process
	while(pos < fsize) {

		//allocate memory for an ip and creates its list of names
		ip = malloc(sizeof(ip));
		ip->names = new_dllist();

		//get ip address
		int i;
		for(i = 0; i < 4; i++) {
			ip->address[i] = fbuf[pos];
			pos++;
		}

		//get numNames
		int tmpInt = 0;
		tmpInt |= fbuf[pos];
		pos++;

		int j;
		for(j = 1; j < 4; j++) {
			tmpInt <<= 8;
			tmpInt |= fbuf[pos];
			pos++;
		}
		numNames = tmpInt;

		//get names
		int len;
		for(j = 0; j < numNames; j++) {
			name    = malloc(sizeof(char)*MAX_NAME_LEN);
			nameArr = realloc(nameArr, sizeof(char *)*(arrCnt+1));

			len = strlen(&fbuf[pos]);
			strcpy(name, &fbuf[pos]);

			//insert into nameArr 
			nameArr[arrCnt] = name;
			arrCnt++;
			
			//insert name into ip->names
			dll_append(ip->names, new_jval_s(name));

			pos += len + 1;

			//if name has a . in it, get local name and put into nameArr
			if(strchr(name, '.') != NULL) {
				tokname = malloc(sizeof(char)*MAX_NAME_LEN);
				strcpy(tokname, name);
				lname = strtok(tokname, "."); 
				nameArr = realloc(nameArr, sizeof(char *)*(arrCnt + 1));
				nameArr[arrCnt] = lname;
				arrCnt++;

				//insert lname into ip->names
				dll_append(ip->names, new_jval_s(lname));
			}
		}

		//traverse ip->names list and insert ip struct ptrs into JRB tree of lists
		dll_traverse(tlist, ip->names) {
			tmp = jrb_find_str(hosts, tlist->val.s);
			if(tmp != NULL) {
				dll_append(tmp->val.v, new_jval_v(ip));
			}
			Dllist newlist;
			tmp = jrb_insert_str(hosts, tlist->val.s, new_jval_v(newlist = new_dllist()));
			dll_append(tmp->val.v, new_jval_v(ip));
		}
	}	

	printf("Hosts all read in\n\n");

	//loop that gets key from user and searches JRB tree for key
	//prints out each matching ip struct and its associated names
	//loop goes until ctrl-d for EOF to quit
	char lk4name[MAX_NAME_LEN];
	int flag;
	while(flag != EOF) {
		printf("Enter host name: ");
		flag = scanf("%s", &lk4name);
		if(flag == EOF) {
			printf("\n");
			continue;
		}
		tmp2 = jrb_find_str(hosts, lk4name);
		if(tmp2 == NULL){
			printf("no key %s\n\n", lk4name);
			continue;
		}
		else {
			tlist = tmp2->val.v;
			dll_traverse(tlist2, tlist) {
				ip = (IP *)tlist2->val.v;
			printf("%d.%d.%d.%d: ", ip->address[0], ip->address[1], ip->address[2], ip->address[3]);
			dll_traverse(tlist3, ip->names) {
				printf("%s ", tlist3->val.s);
			}
			printf("\n\n");
			}
		}
	}

	//deallocate node memory for JRB tree
	jrb_traverse(tmp, hosts){
		tlist = tmp->val.v;
		free(tlist);
	}

	//deallocate file buffer, name array memory, and JRB tree
	int i;	
	for(i = 0; i < arrCnt; i++) {
		free(nameArr[i]);
	}
	free(nameArr);
	free(fbuf);
	jrb_free_tree(hosts);
	
	return(0);
}
