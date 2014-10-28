/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
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
#ifdef KMA_BUD
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
#define MINBUFSIZE 64
#define NUMBEROFBUF PAGESIZE / MINBUFSIZE

#define LEFT_CHILD(n) ((n) * 2 + 1)
#define RIGHT_CHILD(n) ((n) * 2 + 2)
#define PARENT(n) (((n) + 1) / 2 - 1)

#define OFFSET(n, size) (((n) + 1) * (size) - PAGESIZE)
#define IS_TWO_POWER(x) (!((x) & ((x) - 1)))
#define MOD(x, n) ((x) & ((n) - 1))
#define LARGER(x, y) ((x) > (y) ? (x) : (y))

typedef struct {
  kma_page_t* next_page;
  uint8_t large;
  uint16_t longest_length[2 * NUMBEROFBUF - 1];
} page_header_t;

/************Global Variables*********************************************/
kma_page_t* first_page = NULL;

/************Function Prototypes******************************************/
static unsigned int round_size(unsigned int);
void init_header(kma_page_t*);
kma_page_t* find_alloc_page(kma_size_t);
kma_page_t* find_free_page(void*);
unsigned int real_size(unsigned int, unsigned int);
void remove_page(kma_page_t*);
	
/************External Declaration*****************************************/

/**************Implementation***********************************************/

static unsigned int
round_size(unsigned int size)
{
  size = size | (size >> 1);
  size = size | (size >> 2);
  size = size | (size >> 4);
  size = size | (size >> 8);
  size = size | (size >> 16);
  return size + 1;
}

void
init_header(kma_page_t* page)
{
  unsigned int i, node_size = 2 * PAGESIZE;
  unsigned int offset, pre_filled_offset;
  int effective_length;

  page_header_t* page_header;
  page_header = (page_header_t*)(page->ptr);
  page_header->next_page = NULL;
  page_header->large = 0;

  pre_filled_offset = sizeof(page_header_t);

  for (i = 0; i < 2 * NUMBEROFBUF - 1; i++)
  {
    if (IS_TWO_POWER(i + 1)) node_size = node_size / 2;
    
    offset = OFFSET(i, node_size);
    if (offset < pre_filled_offset)
    {
      effective_length = offset + node_size - pre_filled_offset;
      page_header->longest_length[i] = effective_length > 0 ? effective_length : 0;
    } else
      page_header->longest_length[i] = node_size;
  }
}

void*
kma_malloc(kma_size_t size)
{
  unsigned int node_size;
  unsigned int power_size;
  unsigned int offset;
  unsigned int index = 0; 

  kma_page_t* page;
  page_header_t* page_header;

  page = find_alloc_page(size);
  if (page == NULL)
    return NULL;
  else
  {
    page_header = page->ptr;
    if ((size + sizeof(page_header_t)) > PAGESIZE)
    {
      page_header->large = 1;
      return page->ptr + sizeof(kma_page_t*) + sizeof(uint8_t);
    }
  }

  if (!IS_TWO_POWER(size))
    power_size = round_size((unsigned int)size);
  else
    power_size = size;
  
  if (power_size < MINBUFSIZE)
    power_size = MINBUFSIZE;

  for (node_size = PAGESIZE; node_size != power_size; node_size /= 2)
  {
    if (page_header->longest_length[LEFT_CHILD(index)] >= size)
      index = LEFT_CHILD(index);
    else
      index = RIGHT_CHILD(index);
  }

  offset = OFFSET(index, node_size) + node_size - page_header->longest_length[index];
  page_header->longest_length[index] = 0;

  /*printf("ALLOC: Size is %d, offset is %d, index is %d, node_size is %d\n", size, offset, index, node_size);*/

  while (index)
  {
    index = PARENT(index);
    page_header->longest_length[index] = LARGER(page_header->longest_length[LEFT_CHILD(index)],
                                                page_header->longest_length[RIGHT_CHILD(index)]);
  }

  return page->ptr + offset;
}

void 
kma_free(void* ptr, kma_size_t size)
{
  kma_page_t* page = find_free_page(ptr);
  page_header_t* page_header = page->ptr;
  unsigned int left_length, right_length;
  unsigned int node_size, index = 0;
  unsigned int offset;

  if (page_header->large == 1)
  {
    remove_page(page);
    return;
  }
  
  node_size = MINBUFSIZE;
  offset = ptr - page->ptr;
  if (MOD(offset, MINBUFSIZE) != 0)
    offset = offset - MOD(offset, MINBUFSIZE);

  index = (offset + PAGESIZE) / node_size - 1;

  for (; page_header->longest_length[index] != 0; index = PARENT(index))
    node_size = node_size * 2;

  page_header->longest_length[index] = (uint16_t)real_size(index, node_size);

  /*printf("FREE: Size is %d, offset is %d, index is %d, node_size is %d, longest_length is %d\n", size, offset, index, node_size, page_header->longest_length[index]);*/

  while (index)
  {
    index = PARENT(index);
    node_size = node_size * 2;
    left_length = page_header->longest_length[LEFT_CHILD(index)];
    right_length = page_header->longest_length[RIGHT_CHILD(index)];

    if (left_length + right_length == real_size(index, node_size))
      page_header->longest_length[index] = (uint16_t)real_size(index, node_size);
    else
      page_header->longest_length[index] = LARGER(left_length, right_length);
  }

  if (page_header->longest_length[0] == (PAGESIZE - sizeof(page_header_t)))
    remove_page(page);
}

kma_page_t*
find_alloc_page(kma_size_t size)
{
  kma_page_t* page = first_page;
  page_header_t* page_header;

  if ((size + sizeof(kma_page_t*)) > PAGESIZE)
    return NULL;

  if (page == NULL)
  {
    page = get_page();
    init_header(page);
    first_page = page;
    return page;
  }
  else
  {
    while (page != NULL)
    {
      page_header = (page_header_t*)page->ptr;

      if (page_header->large != 1 && page_header->longest_length[0] >= size)
        return page;

      if (page_header->next_page == NULL)
      {
        page_header->next_page = get_page();
        init_header(page_header->next_page);
        return page_header->next_page;
      }
      /*printf("size, %d, length: %d\n", size, page_header->longest_length[0]);*/

      page = page_header->next_page;
    }
  }
  return NULL;
}

kma_page_t*
find_free_page(void* ptr)
{
  kma_page_t* page = first_page;
  page_header_t* page_header;
  int offset;

  while (page)
  {
    page_header = (page_header_t*)(page->ptr);
    offset = ptr - page->ptr;
    if (offset > 0 && offset < PAGESIZE)
      return page;
    page = page_header->next_page;
  }
  return NULL;
}

unsigned int
real_size(unsigned int index, unsigned int node_size)
{
  unsigned int size = node_size;
  unsigned int offset = OFFSET(index, node_size);
  unsigned int occupied_offset = sizeof(page_header_t);

  if (occupied_offset >= offset)
  {
    size = size - (occupied_offset - offset);
  }
  return size;
}

void
remove_page(kma_page_t* page)
{
  kma_page_t* current_page = first_page;
  page_header_t* current_header = first_page->ptr;

  if (page->id == current_page->id)
  {
    first_page = current_header->next_page;
    free_page(page);
  }
  else
  {
    while (current_header->next_page)
    {
      if (current_header->next_page->id == page->id)
      {
        current_header->next_page = ((page_header_t*)(page->ptr))->next_page;
        free_page(page);
        break;
      }
      current_page = current_header->next_page;
      current_header = (page_header_t*)(current_page->ptr);
    }
  }
}
#endif // KMA_BUD
