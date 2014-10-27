/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 ***************************************************************************/
#ifdef KMA_RM
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

typedef struct 
{
  void* next;
  void* prev;
  void* page;
  int size;	
} rm_block;

typedef struct 
{
  //int page_id;
  void* this;
  void* next_page;
  void* first_free_block;
  int page_count;
  int block_count;
  //int max_block;	
} rm_page_head;

/************Global Variables*********************************************/
kma_page_t* page_entry = NULL;
/************Function Prototypes******************************************/
void init_page(kma_page_t *page);

void* find_first_fit(int size); 

void add_block (void* addr, int size);

void remove_block (void* addr);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
  //if size larger than page, return null
  if ((size + sizeof(void *)) > PAGESIZE) {
  	return NULL;
  }		
  if (page_entry == NULL) {
  	kma_page_t* page = get_page();
  	page_entry = page;
  	init_page(page);
  }
  void *first_fit;
  first_fit = find_first_fit(size);

  rm_page_head* page = BASEADDR(first_fit);
  (page -> block_count) ++;

  return first_fit;
}

void
init_page(kma_page_t *page) {

	rm_page_head *pagehead;
	*((kma_page_t**) page->ptr) = page;

	pagehead = (rm_page_head*) (page->ptr);

	//add block ptr to pagehead
	pagehead -> page_count = 0;
	pagehead -> block_count = 0;

	pagehead -> first_free_block = (rm_block*)((long int)pagehead + sizeof(rm_page_head));

	add_block(((void*)(pagehead -> first_free_block)),(PAGESIZE - sizeof(rm_page_head)));

}

void*
find_first_fit(int size) {
	int min_size = sizeof(rm_block);
	if (size < sizeof(rm_block))
		size = sizeof(rm_block);

	rm_page_head *mainpage;
	//search from get the page entry
	mainpage = (rm_page_head*) (page_entry -> ptr);
	//int blocksize;
	//go find the first fit
	rm_block* tmp = ((rm_block *)(mainpage -> first_free_block));

	while (tmp != NULL) {
		if (tmp -> size < size) {
			tmp = tmp -> next;
			continue;
		}
		else if(tmp -> size == size || (tmp -> size - size) < min_size){
			remove_block(tmp);
			return ((void*)tmp);
		}
		else {
			add_block((void*)((long int)tmp + size), (tmp->size - size));
			remove_block((tmp));
			return((void*)tmp);
		}
	}

	kma_page_t* new_page = get_page();
	
	init_page(new_page);
	mainpage -> page_count++;
	return find_first_fit(size);
}

void add_block (void* addr, int size) {

	//set up a new block
	((rm_block*)addr) -> size = size;
	((rm_block*)addr) -> prev = NULL;

	//start from first page
	rm_page_head* mainpage = (rm_page_head*) page_entry -> ptr;
	void* start = (void*) mainpage -> first_free_block;
	//if new block is before first block
	if (addr < start) {
		((rm_block*)(mainpage -> first_free_block))->prev = (rm_block*)addr;
     	((rm_block*)addr) -> next = ((rm_block*)(mainpage -> first_free_block));
      	mainpage -> first_free_block = (rm_block*)addr;
      	return;
	}
	else if (addr == start) {
		((rm_block*)addr) -> next = NULL;
		return;
	}
	else {
		while (((rm_block*)start) -> next != NULL && start < addr) {
			start = ((void*)(((rm_block*)start)->next));
		}

		rm_block* tmp = ((rm_block*)start) -> next;
		if (tmp != NULL)
			tmp -> prev = addr;
		((rm_block*)start) -> next = addr;
		((rm_block*)addr) -> prev = start;
		((rm_block*)addr) -> next = tmp;
	}
}

void remove_block (void* addr) {

	rm_block* ptr = (rm_block*) addr;
	rm_block* ptr_next = ptr -> next;
	rm_block* ptr_prev = ptr -> prev;

	//only one node 
	if (ptr_prev == NULL && ptr_next == NULL) {
		rm_page_head* tmp_page = page_entry -> ptr; 
		tmp_page -> first_free_block = NULL;

		page_entry = 0;
		return;
	}
	//if the block is the last one
	else if (ptr_next == NULL) {
		ptr_prev -> next = NULL;
		return;
	}
	//if the block is the first one
	else if (ptr_prev == NULL) {
		rm_page_head* tmp_page = page_entry -> ptr; 
		ptr_next -> prev = NULL;
		tmp_page -> first_free_block = ptr_next;
		return;
	}
	//if the block is the middle one
	else {
		rm_block* tmp1 = ptr -> prev;
		rm_block* tmp2 = ptr -> next;

		tmp1 -> next = tmp2;
		tmp2 -> prev = tmp1;
		return;
	}
}


void
kma_free(void* ptr, kma_size_t size)
{
	//printf("kma_free\n");
  add_block(ptr, size);
  rm_page_head* base_addr = BASEADDR(ptr);
  base_addr -> block_count = base_addr -> block_count - 1;

  rm_page_head* first_page = (rm_page_head*)(page_entry -> ptr);
  int end = first_page -> page_count;
  int run = 1;

  rm_page_head* last_page;
  for (; run; end--) {
    last_page = (((rm_page_head*)((long int)first_page + end * PAGESIZE))); // Get Last Page
    run = 0;
    if(last_page -> block_count == 0){
      run = 1;
      
      rm_block* tmp;
      for(tmp = first_page -> first_free_block; tmp != NULL; tmp = tmp->next)
		if(BASEADDR(tmp) == last_page)
	  		remove_block(tmp);
      
      run = 1;      
      if(last_page == first_page){
		run = 0;
		page_entry = NULL;
  		#ifdef LOGGING
      		printf("%f\n",totalUtil/(float)numrequests);
    	#endif
      }

      // printf("1\n");
      free_page(last_page -> this);
      // printf("2\n");
      if(page_entry != NULL)
		first_page -> page_count -= 1;
    }
  }
  #ifdef LOGGING
    totalRequests-=size;
  #endif
}
#endif // KMA_RM






