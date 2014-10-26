/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the power-of-two free list
 *             algorithm
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.2 $
 *    Last Modification: $Date: 2009/10/31 21:28:52 $
 *    File: $RCSfile: kma_p2fl.c,v $
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: kma_p2fl.c,v $
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
#ifdef KMA_P2FL
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>


/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

typedef struct bufferT
{
    kma_size_t size;
    struct bufferT* next_size; //link to the next header
    struct bufferT* next_buffer; // link to the next buffer
    kma_page_t* page;
} buffer_t;

/************Global Variables*********************************************/
static buffer_t* buffer_list = NULL;
const int MINBLOCKSIZE = 32;
/************Function Prototypes******************************************/
kma_size_t choose_block_size(kma_size_t);
void deinit_buffer_list(void);
void init_buffer_list(void);
void* alloc_block(kma_size_t);
buffer_t* make_buffers(kma_size_t);
int last_buf_in_page(buffer_t* size_buf, kma_page_t* page);
void free_page_from_list(buffer_t* size_buf, kma_page_t* page);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
    if(buffer_list == NULL)
        init_buffer_list();
    kma_size_t block_size = choose_block_size(size);
    if(block_size != -1)
        return alloc_block(block_size);
    return NULL;
}

/* Go through buffer_list until block_size is found. Then find first buffer_t
 * on free list, take it off free list and return its ptr + sizeof(buffer_t*)
 */
void*
alloc_block(kma_size_t block_size)
{
    buffer_t* top = buffer_list;
    while(top->size < block_size)
        top = top->next_size;
    buffer_t* buf = top->next_buffer;
    if(buf == NULL)
    {
        buf = make_buffers(top->size);
        //if no more buffer can be made
        if(buf == NULL)
          return NULL;
    }

    top->next_buffer = buf->next_buffer;
    //when the buffer is allocated, the pointer point to the size header
    buf->next_buffer = top; 
    return ((void*)buf + sizeof(buffer_t));
}

kma_size_t choose_block_size(kma_size_t size) {
    int test_size = MINBLOCKSIZE;
    while(test_size <= PAGESIZE)
    {
        if(test_size >= (size + sizeof(buffer_t)))
            return test_size;
        test_size *= 2;
    }
    return -1;
}

void deinit_buffer_list(void) {
    free_page(buffer_list->page);
    buffer_list = NULL;
}

void
init_buffer_list(void)
{
    //this is where we should be bringing down the initial control page
    kma_page_t* page = get_page();
    buffer_list = page->ptr;
    buffer_t* current = page->ptr;

    int offset = sizeof(buffer_t);

    current->next_buffer = page->ptr + offset;
    current->page = page;
    current->size = 0;
    //current->ptr = NULL;
    offset += sizeof(buffer_t);
    int size = MINBLOCKSIZE;
    while(size <= PAGESIZE)
    {
        current->next_size = page->ptr + offset;
        current = current->next_size;
        current->next_buffer = NULL;
        current->size = size;
        current->page = page;
        size *= 2;
        offset += sizeof(buffer_t);
    }
    current->next_size = NULL;
}

buffer_t* make_buffers(kma_size_t size)
{
    kma_page_t* page = get_page();
    if(page == NULL)
        return NULL;

    buffer_list->next_buffer->size++;
    buffer_t* top = page->ptr;

    top->next_size = NULL;
    top->size = size;
    top->page = page;
    int offset = size;
    buffer_t* current;

    while(offset < PAGESIZE)
    {
        current = page->ptr + offset;
        top->next_buffer = current;
        top = current;
        offset += size;
        current->next_size = NULL;
        current->size = size;
        current->page = page;
    }
    top->next_buffer = NULL;
    return (buffer_t *) page->ptr;
}



void
kma_free(void* ptr, kma_size_t size)
{
    buffer_t* buf;
    buf = (buffer_t*)(ptr - sizeof(buffer_t));
    buffer_t* size_buf = buf->next_buffer;
    buf->next_buffer = size_buf->next_buffer;
    size_buf->next_buffer = buf;

    if(last_buf_in_page(size_buf, buf->page) == 1)
        free_page_from_list(size_buf, buf->page);
    if(buffer_list->next_buffer->size == 0)
        deinit_buffer_list();
}

void
free_page_from_list(buffer_t* size_buf, kma_page_t* page)
{
    buffer_t* prev = size_buf;
    buffer_t* top = size_buf->next_buffer;
    while(top != NULL)
    {
        if(top->page == page)
        {
            while(top != NULL && top->page == page)
            {
                top = top->next_buffer;
            }
            if(top != NULL)
            {
                prev->next_buffer = top;
                prev = top;
                top = top->next_buffer;
            }
            else
                prev->next_buffer = NULL;
        }
        else
        {
            prev = top;
            top = top->next_buffer;
        }
    }
    buffer_list->next_buffer->size--;
    free_page(page);
}

int
last_buf_in_page(buffer_t* size_buf, kma_page_t* page)
{
    buffer_t* top = size_buf->next_buffer;
    kma_size_t used_buffs_size = 0;
    while(top != NULL)
    {
        if(top->page == page)
            used_buffs_size += size_buf->size;
        top = top->next_buffer;
    }
    if (used_buffs_size >= PAGESIZE)
        return 1;
    return 0;
}


#endif // KMA_P2FL

