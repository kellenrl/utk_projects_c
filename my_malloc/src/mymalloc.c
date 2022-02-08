#include "mymalloc.h"

// C
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

//block info, used in 2 ways:
//Allocated -> only size is used
//Free -> both size and flink used
struct block {
	//size is used for both allocated and free frames
	unsigned int size;

	//flink is only legitimate in the free list
	//NOT legitimate if this block is allocated
	struct block *flink;
};

//compare function for struct block *'s -> used for qsort
int blockCompare(const void *ptr1, const void *ptr2) {
	struct block *a = *((struct block **)ptr1);
	struct block *b = *((struct block **)ptr2);
	if(a < b) return -1;
	if(a > b) return 1;
	return 0;
}

//GLOBAL VARIABLE
//points to the head of the free list
struct block *fl_head = NULL;

//name - my_malloc
//brief - allocates size bytes of memory and returns void* to the allocated block
//param[in] - size_t size - number of bytes to be allocated
//return - void* to the first byte of the allocated memory block
void* my_malloc(size_t size) {

	if((int)size < 1) {
		return NULL;
	}

	struct block *free_ptr;
	struct block *alloc_ptr;

	//align size to 8 bytes
	size = (size + 7 + 8) & -8;

	//check to see if free list is empty
	if(fl_head == NULL) {
		//if no memory currently available, get some and make free_ptr point to it
		//if size is larger than 8192, get size chunk of memory
		if(size > 8192) {
			free_ptr = (struct block *)sbrk(size);

			//error if no memory left to grab
			if(free_ptr == sbrk(0)) {
				fprintf(stderr, "No memory left in the heap, exiting\n");
				exit(1);
			}
			alloc_ptr = free_ptr;

			//set alloc_ptr
			alloc_ptr->size = size;
			alloc_ptr = (struct block *) ((char *)free_ptr + 8);
			//using entire size block so nothing is added to free list
		}
		//else get 8192 bytes
		else {
			free_ptr = sbrk(8192);
			if(free_ptr == sbrk(0)) {
				fprintf(stderr, "No memory left in the heap, exiting\n");
				exit(1);
			}
			alloc_ptr = free_ptr;

			//since we are not using all of these bytes and free list is empty,
			//set free list head to point after this block
			free_ptr = (struct block *) ((char *)free_ptr + size);
			free_ptr->size = 8192 - size;
			fl_head = free_ptr;

			//set alloc_ptr to free_ptr + 8 and its size
			alloc_ptr->size = size;
			alloc_ptr = (struct block *) ((char *)alloc_ptr + 8);
		}
		//set alloc_ptr's flink to NULL before returning to user
		alloc_ptr->flink = NULL;
		return((void *)alloc_ptr);
	}

	//free list is not empty
	else {
		struct block *tmpBlock= NULL;
		struct block *foundBlock= NULL;
		struct block *prevBlock = NULL;
	
		//find a block big enough for request
		tmpBlock = fl_head;
		if(tmpBlock->size >= size) foundBlock = tmpBlock;
		else {
			while(tmpBlock->flink != NULL) {
				prevBlock = tmpBlock;
				tmpBlock = tmpBlock->flink;
				if(tmpBlock->size >= size) {
					foundBlock = tmpBlock;
					break;
				} 
			}
		}
		if(foundBlock == NULL) {
			//if no memory block big enough, get some and make free_ptr point to it
			//if size is larger than 8184, get size chunk of memory
			if(size > 8192) {
				free_ptr = (struct block *)sbrk(size);

				//error if no memory left to grab
				if(free_ptr == sbrk(0)) {
					fprintf(stderr, "No memory left in the heap, exiting\n");
					exit(1);
				}
				//set the size of this block
				free_ptr->size = size;

				//set alloc_ptr
				alloc_ptr = (struct block *) ((char *)free_ptr + 8);
				//using entire size block so nothing is added to free list
			}
			//else get 8192 bytes
			else {
				struct block *new_free_ptr;

				free_ptr = sbrk(8192);
				if(free_ptr == sbrk(0)) {
					fprintf(stderr, "No memory left in the heap, exiting\n");
					exit(1);
				}
				free_ptr->size = 8192;
				new_free_ptr = (struct block *) ((char *)free_ptr + size);
				new_free_ptr->size = free_ptr->size - size;
				new_free_ptr->flink = fl_head;
				fl_head = new_free_ptr;
	
				//set alloc_ptr to free_ptr + 8 and its size
				free_ptr->size = size;
				alloc_ptr = (struct block *) ((char *)free_ptr + 8);
			}
			//set alloc_ptr's flink to NULL before returning to user
			alloc_ptr->flink = NULL;
			return((void *)alloc_ptr);
		}
		//foundBlock is a block big enough for request
		else {
			free_ptr = foundBlock;

			//if block is exactly the right size
			if(foundBlock->size == size) {
				if(foundBlock == fl_head) fl_head = fl_head->flink;
				if(prevBlock != NULL) prevBlock->flink = foundBlock->flink;
				
				alloc_ptr = (struct block *) ((char *)foundBlock + 8);
				alloc_ptr->flink = NULL;

				return((void *)alloc_ptr);
			}
			else {
				//need to carve off memory from the block
				free_ptr = (struct block *) ((char *)foundBlock + size);
				free_ptr->size = foundBlock->size - size;
				free_ptr->flink = fl_head->flink;
				foundBlock->size = size;

				if(foundBlock == fl_head) fl_head = free_ptr;
				else if(prevBlock != NULL) {
					prevBlock->flink = free_ptr;
					free_ptr->flink = foundBlock->flink;
				} 
				
				alloc_ptr = (struct block *) ((char *)foundBlock + 8);
				alloc_ptr->flink = NULL;

				return((void *)alloc_ptr);
			}
		}
	}
}

//name - my_free
//brief - deallocates memory pointed to by parameter and adds to the free list
//param[in] - void *ptr - points to memory block to be deallocated
void my_free(void* ptr) {
	struct block *tmp = (struct block *)((char *)ptr - 8);
	if(fl_head == NULL) fl_head = tmp;
	else {
		tmp->flink = fl_head;
		fl_head = tmp;
	}
}

//name - free_list_begin
//brief - returns a pointer to the head of the free list, global variable fl_head
//return - a pointer to the head of the free list, global variable fl_head or NULL if free list empty
void* free_list_begin() {
	if(fl_head != NULL) return(fl_head);
	else { return(NULL); }
}

//name - free_list_next
//brief - returns a pointer to the next node on the free list or NULL if there isn't another block
//param[in] - void *node - the node we want to flink for
//return - pointer to parameters flink, NULL if parameter has no flink
void* free_list_next(void* node) {
	struct block *block = (struct block *)node;
	if(block->flink == NULL) return(NULL);
	else { return(block->flink); }
}

//name - coalesce_free_list
//brief - sorts free list blocks by memory address, consolidates all contiguous blocks 
void coalesce_free_list() {
	//count the number of blocks on the free list
	int count = 1;
	struct block *tmp = fl_head;
	while(tmp->flink != NULL) {
		count++;
		tmp = tmp->flink;
	}	
	//create array of count struct block *'s
	struct block *arr[count];
	
	//load array with all structs on free list
	tmp = fl_head;
	int i = 0;
	arr[i] = tmp;
	while(tmp->flink != NULL) {
		i++;
		tmp = tmp->flink;
		arr[i] = tmp;
	}

	//sort array by memory address of the blocks
	qsort(arr, count, sizeof(struct block *), blockCompare);

	//rebuild free list with blocks in order
	fl_head = arr[0];
	fl_head->flink = arr[1];
	tmp = fl_head->flink;
	for(i = 2; i < count; i++) {
		tmp->flink = arr[i];
		tmp = tmp->flink;	
	}	
	tmp->flink = NULL;

	//consolidate contiguous blocks
	tmp = fl_head;
	while(tmp->flink != NULL) {
		struct block *check = (struct block *) ((char *)tmp + tmp->size);
		if(check == tmp->flink) {
			tmp->size += tmp->flink->size;
			tmp->flink = tmp->flink->flink;
		}
		else {
			tmp = tmp->flink;
		}
	}
}