#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fields.h"
#include "jval.h"
#include "dllist.h"
#include "jrb.h"

typedef struct Person {
	char *name;
	char sex;
	struct Person *father;
	struct Person *mother;
	struct Person **children;
	int nkids;
	int visited;
	int printed;
} Person;

void prettyPrint(Person *);
int is_descendant(Person *);
char* getName(IS); 
Person* addPerson(JRB, char*);
void makeLinks(IS, Person *, Person*);
void addChild(Person *, Person*);

int main (int argc, char *argv[]) {
	JRB people;

	//temp variables
	IS is;
	int i, nlines, nkids;
	Person *p, *pp, *parent, *child;
	char *name;
	JRB tmp;
	int nsize;

	is = new_inputstruct(NULL);
	people = make_jrb();
	nlines = 0;

	//while there is still input
	while(get_line(is) >= 0) { 
		nlines++;

		//if this is empty line, continue
		if(is->NF == 0) continue; 		

		//if the first word is PERSON, checks if in tree. if not, adds them.
		if(strcmp(is->fields[0], "PERSON") == 0) { 

			//gets full name
			name = getName(is); 

			//p will point to PERSON until next PERSON is read
			tmp = jrb_find_str(people, name);
			if(tmp == NULL) {
				p = addPerson(people, name);
			}
			else {
				p = (Person *)tmp->val.v;
			}
		}

		//if the first word is FATHER, checks if in tree, if not adds. checks if links exist, if not adds
		else if(strcmp(is->fields[0], "FATHER") == 0) {
			name = getName(is);

			//checks if father is already in the tree. if not, adds them.
			tmp = jrb_find_str(people, name);
			if(tmp == NULL) { 
				parent = addPerson(people,name);
				parent->sex = 'M';
			}
			else {
				parent = (Person *)tmp->val.v;

				//if already in tree, check for sex mismatch
				if(parent->sex == 'F') { 
					fprintf(stderr,"Bad input - sex mismatch on line %d\n", nlines);
					return 1;
				}

				//if no error, assign sex since father
				parent->sex = 'M'; 
			}

			//if p already has a father, check to see if is correct
			if(p->father != NULL) { 
				int match;
				match = strcmp(p->father->name, parent->name);
				if(match != 0) {
					fprintf(stderr,"Bad input -- child with two fathers on line %d\n", nlines);
					return 1;
				}
			}

			//if doesnt already have father, make connections
			else { 
				makeLinks(is, parent, p);
			}
		}

		//if the first word is MOTHER, checks if in tree, if not adds. checks if links exist, if not adds
		else if(strcmp(is->fields[0], "MOTHER") == 0) { 
			name = getName(is);
			
			//if not already in tree, add to tree
			tmp = jrb_find_str(people, name);
			if(tmp == NULL) { 
				parent = addPerson(people,name);
				parent->sex = 'F';
			}
			else {
				parent = (Person *)tmp->val.v;

				//if already in tree, check for sex mismatch
				if(parent->sex == 'M') { 
					fprintf(stderr,"Bad input - sex mismatch on line %d\n", nlines);
					return 1;
				}

				//if no error, assign sex since mother
				parent->sex = 'F'; 
			}

			//if p already has a father, check to see if is correct
			if(p->mother != NULL) { 
				int match;
				match = strcmp(p->mother->name, parent->name);
				if(match != 0) {
					fprintf(stderr,"Bad input -- child with two mothers on line %d\n", nlines);
					return 1;
				}
			}

			//if doesnt already have mother, make connections
			else { 
				makeLinks(is, parent, p);
			}
		}	

		//if first word is FATHER_OF checks if in tree and processes person
		else if(strcmp(is->fields[0], "FATHER_OF") == 0) {
			name = getName(is);

			//checks if in tree, if not adds them
			tmp = jrb_find_str(people, name);
			if(tmp == NULL) {
				child = addPerson(people, name);
			}
			else {
				child = (Person *)tmp->val.v;
			}

			//if sex value not already set, assigns value
			if(p->sex == '\0') p->sex = 'M';

			//checks for sex error
			if(p->sex == 'F') {
				fprintf(stderr,"Bad input - sex mismatch on line %d\n", nlines);
				return 1;
			}

			//if child already has father, check if correct
			if(child->father != NULL) { 
				int match;
				match = strcmp(child->father->name, p->name);
				if(match != 0) {
					fprintf(stderr,"Bad input -- child with two fathers line %d\n", nlines);
					return 1;
				}
			}

			//make connections
			else { 
				makeLinks(is, p, child);
			}
		}

		//if first word is FATHER_OF checks if in tree and processes person
		else if(strcmp(is->fields[0], "MOTHER_OF") == 0) {
			name = getName(is);

			//checks if in tree, if not adds them
			tmp = jrb_find_str(people, name);
			if(tmp == NULL) {
				child = addPerson(people, name);
			}
			else {
				child = (Person *)tmp->val.v;
			}

			//if sex value not already set, assigns value
			if(p->sex == '\0') p->sex = 'F';

			//checks for sex error
			if(p->sex == 'M') {
				fprintf(stderr,"Bad input - sex mismatch on line %d\n", nlines);
				return 1;
			}

			//if child already has mother, check if correct
			if(child->mother != NULL) { 
				int match;
				match = strcmp(child->mother->name, p->name);
				if(match != 0) {
					fprintf(stderr, "Bad input -- child with two mothers on line %d\n", nlines);
					return 1;
				}
			}

			//make connections
			else { 
				makeLinks(is, p, child);
			}
		}	

		//if first word is SEX, checks for sex errors and assigns sex value to Person *p
		else if(strcmp(is->fields[0], "SEX") == 0) {
			if(p->sex == '\0') p->sex = *is->fields[1];
			if(p->sex != *is->fields[1]) {
				fprintf(stderr,"Bad input - sex mismatch on line %d\n", nlines);
				return 1;
			}
		}
	}

	//creates and populates dllist with people without parents, checks for cycle errors
	Dllist toprint = new_dllist();
	Dllist tlist;
	jrb_traverse(tmp, people) {
		pp = (Person *)tmp->val.v;
		if(is_descendant(pp)) {
			fprintf(stderr, "Bad input -- cycle in specification\n");
			return 1;
		}
		if(pp->father == NULL && pp->mother == NULL) {
			dll_append(toprint, new_jval_v(pp));
		}
	}
	
	//populates the rest of the dllist and prints people in correct order
	while(dll_empty(toprint) != 1) {
		tlist = toprint->flink;
		p = (Person *)tlist->val.v;
		dll_delete_node(dll_first(toprint));
		if(p->printed == 0) {
			if((p->father == NULL || p->mother == NULL) || (p->father->printed > 0 && p->mother->printed > 0)) {
				prettyPrint(p);
				p->printed++;
				int i;
				for(i = 0; i < p->nkids; i++) {
					pp = p->children[i];
					dll_append(toprint, new_jval_v(pp));
				}
			}
		}
	}

	//memory deallocation
	jrb_traverse(tmp, people) {
		int i;
		pp = (Person *)tmp->val.v;
		free(pp->name);
		free(pp->children);
		free(pp);
	}
	free_dllist(toprint);
	jrb_free_tree(people);
	jettison_inputstruct(is);

	return 0;
}

//name - is_descendant
//param[in] - Person * to check for cycles
//return - 1 if cycle error exists, 0 if no errors
int is_descendant(Person *p)
{
	int i;
    if (p->visited == 1) return 0;  /* I.e. we've processed this person before and he/she's ok */
    if (p->visited == 2) return 1;  /* I.e. the graph is messed up */
    p->visited = 2;
	for(i = 0; i < p->nkids; i++){
		if(is_descendant(p->children[i])) return 1;
	}
    p->visited = 1;
    return 0;
}

//name - prettyPrint
//param[in] - Person * to print
//post - prints formatted Person, their info, and children
void prettyPrint(Person *p)
{
	int i;
	if(p->name != NULL) printf("%s\n", p->name);
	if(p->sex == '\0') printf("  Sex: Unknown\n");
	if(p->sex == 'F') printf("  Sex: Female\n");
	if(p->sex == 'M') printf("  Sex: Male\n");
	if(p->father == NULL) printf("  Father: Unknown\n");
	else printf("  Father: %s\n", p->father->name);
	if(p->mother == NULL) printf("  Mother: Unknown\n");
	else printf("  Mother: %s\n", p->mother->name);
	if(p->nkids > 0) {
		printf("  Children:\n");
		for(i = 0; i < p->nkids; i++) {
			printf("    %s\n", p->children[i]->name);
		}
	}
	else printf("  Children: None\n");
	printf("\n");
}

//name - getName
//param[in] - IS inputstructure
//return - char* to persons full name
char* getName(IS is)
{
	int i, nsize;
	char *name;

	nsize = strlen(is->fields[1]);
	for(i = 2; i < is->NF; i++) nsize += (strlen(is->fields[i])+1);
		name = (char *)malloc(sizeof(char)*(nsize+1));
		strcpy(name, is->fields[1]);

		nsize = strlen(is->fields[1]);
		for (i = 2; i < is->NF; i++) {
			name[nsize] = ' ';
			strcpy(name+nsize+1, is->fields[i]);
			nsize += strlen(name+nsize);
		}
	return(name);		
}

//name - addPerson
//param[in] - JRB tree to add to
//param[in] - name of Person to add to JRB tree
//returns = pointer to person added, or pointer to person who was already in tree
Person* addPerson(JRB people, char* name)
{
	Person *p;
	int nsize;

	nsize = strlen(name);
	p = malloc(sizeof(Person));
	p->name = (char *)malloc(sizeof(char)*(nsize+1));
	strcpy(p->name, name);
	p->sex = '\0';
	p->father = NULL;
	p->mother = NULL;
	p->children = NULL;
	p->nkids = 0;
	p->visited = 0;
	p->printed = 0;
	jrb_insert_str(people, p->name, new_jval_v(p));

	free(name);

	return(p);
}

//name - addChild
//param[in] - Person *parent, person to add child
//param[in] - Person *child, person to be added to parent
//post - parent now has pointer to child in their **children
void addChild(Person *parent, Person *child)
{
	parent->nkids++;
	parent->children = realloc(parent->children,sizeof(Person *)*parent->nkids);
	parent->children[parent->nkids-1] = malloc(sizeof(Person *));
	parent->children[parent->nkids-1] = child; //add child to father
}

//name - makeLinks
//param[in] - IS inputstructure
//param[in] - Person *parent
//param[in] - Person *child
//post - makes links from parent to child and child to parent if they dont already exist
void makeLinks(IS is, Person *parent, Person *child)
{
	if(strcmp(is->fields[0], "FATHER") == 0 || strcmp(is->fields[0], "FATHER_OF") == 0) {
		child->father = parent;
		addChild(parent, child);
	}
	else {
		child->mother = parent;
		addChild(parent, child);
	}
}
