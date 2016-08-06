//
//  COMP1927 Assignment 1 - Vlad: the memory allocator
//  allocator.c ... implementation
//
//  Created by Liam O'Connor on 18/07/12.
//  Modified by John Shepherd in August 2014, August 2015
//  Copyright (c) 2012-2015 UNSW. All rights reserved.
//

#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define HEADER_SIZE    sizeof(struct free_list_header)
#define MAGIC_FREE     0xDEADBEEF
#define MAGIC_ALLOC    0xBEEFDEAD
#define TRUE 1
#define FALSE 0

typedef unsigned char byte;
typedef u_int32_t vlink_t;
typedef u_int32_t vsize_t;
typedef u_int32_t vaddr_t;

typedef struct free_list_header {
   u_int32_t magic;  // ought to contain MAGIC_FREE
   vsize_t size;     // # bytes in this block (including header)
   vlink_t next;     // memory[] index of next free block
   vlink_t prev;     // memory[] index of previous free block
} free_header_t;

// Global data

static byte *memory = NULL;   // pointer to start of allocator memory
static vaddr_t free_list_ptr; // index in memory[] of first block in free list
static vsize_t memory_size;   // number of bytes malloc'd in memory[]

//define my functions
vsize_t round_up_po2 (vsize_t size);
free_header_t* addr_to_p(vaddr_t free_list_ptr);
free_header_t *new_header (u_int32_t position);
vaddr_t p_to_addr(free_header_t* pointer);
free_header_t* find_last(vaddr_t free_list_ptr);
int find_freeslots(vaddr_t free_list_ptr);

//////////////////////////////////
/////
///// Pre defined fiunctions
/////
/////////////////////////////////

// Input: size - number of bytes to make available to the allocator
// Output: none
// Precondition: Size is a power of two.
// Postcondition: `size` bytes are now available to the allocator
//
// (If the allocator is already initialised, this function does nothing,
//  even if it was initialised with different size)

void vlad_init(u_int32_t size)
{

  if (memory != NULL) return;

//allocate(malloc) inital memory to power of 2
  memory_size = round_up_po2(size);

  //force min memory size to be 512, according to spec
  if (memory_size<512){
    memory_size=512;
  }

  memory = malloc(memory_size);

  if (memory==NULL) {
    fprintf(stderr, "vlad_init: insufficient memory");
    abort();
  }

  //create and assign values to top header

   free_list_ptr =0;

   free_header_t *topHeader = (free_header_t *)&memory[free_list_ptr];

   topHeader->magic = MAGIC_FREE;
   topHeader->size = memory_size;
   topHeader->next = free_list_ptr;
   topHeader->prev= free_list_ptr;

}


// Input: n - number of bytes requested
// Output: p - a pointer, or NULL
// Precondition: n is < size of memory available to the allocator
// Postcondition: If a region of size n or greater cannot be found, p = NULL
//                Else, p points to a location immediately after a header block
//                      for a newly-allocated region of some size >=
//                      n + header size.

void *vlad_malloc(u_int32_t n)
{

  if(memory_size < n+HEADER_SIZE){ //case where requested size is bigger than memory
    return NULL;
  }

  int size = round_up_po2(n+HEADER_SIZE);

  free_header_t * first =  addr_to_p(free_list_ptr);
  free_header_t * curr =  addr_to_p(free_list_ptr);
  free_header_t * currnext = addr_to_p(curr->next);
  free_header_t * bestFit = NULL;

  //find the best location to allocate //O(n)

  do{
    if( curr->size>=size && curr->magic == MAGIC_FREE){
      if (bestFit==NULL || curr->size < bestFit->size){ //if no current bestfit, or if curr is greater than size req but less than current bestfit
        bestFit = curr;
      }
    }
    curr = currnext;
    currnext = addr_to_p(curr->next);
  }while(curr != first);

  if(bestFit==NULL) return NULL;  //if no bestfit found

  if(find_freeslots(free_list_ptr)==1 && bestFit->size==size){  //never allocate the only free slot in memory
    return NULL;
  }

//logarithmic
  while(bestFit->size/2 >= size){//half bestfit slot until it is a suitible size
    struct free_list_header * newHeader = new_header(p_to_addr(bestFit)+bestFit->size/2);

    newHeader->magic = MAGIC_FREE;
    newHeader->size = bestFit->size/2;
    newHeader->prev = p_to_addr(bestFit);
    newHeader->next = bestFit->next;

    free_header_t * next = addr_to_p(bestFit->next);
    next->prev = p_to_addr(newHeader);

    bestFit->next = p_to_addr(newHeader);

    bestFit->size = (bestFit->size)/2;
  }

  //allocate and return bestfit, taking into consideration header size
  bestFit->magic = MAGIC_ALLOC;

  free_header_t * next = addr_to_p(bestFit->next);
  free_header_t * prev = addr_to_p(bestFit->prev);
  prev->next = p_to_addr(next);
  next->prev = p_to_addr(prev);

  curr =  addr_to_p(free_list_ptr);

  if(curr->magic == MAGIC_ALLOC){
    free_list_ptr = curr->next;
  }

  return (void*)bestFit + HEADER_SIZE;

}


// Input: object, a pointer.
// Output: none
// Precondition: object points to a location immediately after a header block
//               within the allocator's memory.
// Postcondition: The region pointed to by object can be re-allocated by
//                vlad_malloc

void vlad_free(void *object)
{
//define things
  free_header_t * insert = addr_to_p(p_to_addr(object)-HEADER_SIZE);
  free_header_t * first =  addr_to_p(free_list_ptr);
  free_header_t * last =  NULL;
  free_header_t * curr =  addr_to_p(free_list_ptr);
  free_header_t * currtemp =  addr_to_p(free_list_ptr);
  free_header_t * currnext = addr_to_p(curr->next);
  free_header_t * currprev = addr_to_p(curr->prev);
  int runs = 0;

  insert->magic = MAGIC_FREE;

//  find where to insert freed object n

  while(p_to_addr(insert)>p_to_addr(curr)){
    curr = currnext;
    currnext = addr_to_p(curr->next);
    currprev = addr_to_p(curr->prev);
    runs++;
    if (curr == first){ //means element being freed is at end of list, since nothing is smaller and goes back to list
      break;
    }
  }

  //shifts pointers around to item item being freed
  insert->next = p_to_addr(curr);
  insert->prev = p_to_addr(currprev);
  currprev->next = p_to_addr(insert);
  curr->prev = p_to_addr(insert);

  if(runs == 0){  //if runs is 0,  prev. loop did  not run meaning it is the first item in the loop, so free_list_ptr needs to be changed
    free_list_ptr = p_to_addr(insert);
  }

//START MERGE ALGORITHM

  int continueRight = TRUE;
  int continueLeft = TRUE;

  curr = insert;
  currnext = addr_to_p(curr->next);
  currprev = addr_to_p(curr->prev);
  first =  addr_to_p(free_list_ptr);
  last =  find_last(free_list_ptr);

//check whether curr can merge left or right until it can not merge left or right n
  while(continueLeft==TRUE || continueRight==TRUE){
    //assume can not merge for next loop unless something merges in this loop
    continueLeft = FALSE;
    continueRight = FALSE;

    if (currnext->size == curr->size && currnext->magic == MAGIC_FREE && curr!=currnext && currnext!=first){
      //CHECK IF CURR AND CURRNEXT ARE ACTUALLY NEXT TO EACH OTHER
      if (p_to_addr(curr)+curr->size == p_to_addr(currnext)) {
        //check to not merge with stuff not split from using funky math to label different cells
        if((p_to_addr(curr)/curr->size)%2==0){
          continueRight = TRUE;
          currtemp = currnext;
          curr->next = currnext->next;
          currnext = addr_to_p(currtemp->next);
          currnext->prev = p_to_addr(curr);
          curr->size = curr->size*2;
          currnext = addr_to_p(curr->next);
          currprev = addr_to_p(curr->prev);
        }
      }
    }
    if (currprev->size == curr->size && currprev->magic == MAGIC_FREE && curr!=currprev && currprev!=last){
      //CHECK IF CURR AND CURRPREV ARE ACTUALLY NEXT TO EACH OTHER
      if (p_to_addr(currprev)+currprev->size == p_to_addr(curr)) {
        //check to not merge with stuff not split from using funky math to label different cells
        if((p_to_addr(curr)/curr->size)%2==1){
          continueLeft = TRUE;
          currprev->next = curr->next;
          currnext->prev = curr->prev;

          currprev->size = currprev->size*2;

          curr = currprev;
          currnext = addr_to_p(curr->next);
          currprev = addr_to_p(curr->prev);
        }
      }
    }

  }

  return;
}


// Stop the allocator, so that it can be init'ed again:
// Precondition: allocator memory was once allocated by vlad_init()
// Postcondition: allocator is unusable until vlad_int() executed again

void vlad_end(void)
{
  free(memory); //free memory
  memory = NULL; //incase other functions try access it
}


// Precondition: allocator has been vlad_init()'d
// Postcondition: allocator stats displayed on stdout

void vlad_stats(void)
{

  int x = 2;
  free_header_t * first =  addr_to_p(free_list_ptr);
  free_header_t * curr =  addr_to_p(free_list_ptr);
  free_header_t * currnext = addr_to_p(curr->next);
  //print addr,size,next,orev anf magic of each block
  fprintf(stdout, "Free Block(s):\n" );
  fprintf(stdout,"1: addr = %d, size = %d,next = %d,prev = %d,magic = %s\n",
  p_to_addr(curr),curr->size,curr->next,curr->prev,(curr->magic == MAGIC_ALLOC) ? "ALLOC" : "FREE" );
  curr = currnext;
  currnext = addr_to_p(curr->next);

  while(curr!=first){
    fprintf(stdout,"%d: addr = %d, size = %d,next = %d,prev = %d,magic = %s\n",
    x,p_to_addr(curr),curr->size,curr->next,curr->prev,(curr->magic == MAGIC_ALLOC) ? "ALLOC" : "FREE");
    curr = currnext;
    currnext = addr_to_p(curr->next);
    x++;
  }
   return;
}

//////////////////////////////////
/////
///// My functions
/////
/////////////////////////////////

vsize_t round_up_po2 (vsize_t size){
  int testPowerUntil = 30;
  int newSize = 0;
  for (int i = 0; i < testPowerUntil; i++) {
    int pO2 = 2 << (i-1); //use bit shifting to find closest power of 2
    if (pO2 == size){
      newSize = size;
      break;
    }else if(pO2 >= size){
      newSize = pO2;
      break;
    }
  }

  if (newSize==0) {
    fprintf(stderr, "round_up_po2: insufficient memory");
    abort();
  }
  return newSize;
}


free_header_t *new_header (u_int32_t position) {
    free_header_t *newHeader = (free_header_t *) &memory[position];
    return newHeader;
}

free_header_t* addr_to_p(vaddr_t addr){  //convert address to an address
  return (free_header_t *)&memory[addr];
}

vaddr_t p_to_addr(free_header_t* pointer){  //convert pointer to an address
  return (vaddr_t)((vlink_t)pointer -(vlink_t)memory);
}

free_header_t* find_last(vaddr_t free_list_ptr){

  free_header_t * first =  addr_to_p(free_list_ptr);
  free_header_t * curr =  addr_to_p(free_list_ptr);
  free_header_t * currnext = addr_to_p(curr->next);
  //loop through list until currnext elements is first element, to find list last
  while(currnext!=first){
    curr = currnext;
    currnext = addr_to_p(curr->next);
  }

  return curr;
}

int find_freeslots(vaddr_t free_list_ptr){

  free_header_t * first =  addr_to_p(free_list_ptr);
  free_header_t * curr =  addr_to_p(free_list_ptr);
  free_header_t * currnext = addr_to_p(curr->next);
  int size = 1;
  //loop through list until curent elements is first element again, to find list size
  while(currnext!=first){
    curr = currnext;
    currnext = addr_to_p(curr->next);
    size++;
  }

  return size;
}

//
// All of the code below here was written by Alen Bou-Haidar, COMP1927 14s2
//

//
// Fancy allocator stats
// 2D diagram for your allocator.c ... implementation
//
// Copyright (C) 2014 Alen Bou-Haidar <alencool@gmail.com>
//
// FancyStat is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// FancyStat is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>


#include <string.h>

#define STAT_WIDTH  32
#define STAT_HEIGHT 16
#define BG_FREE      "\x1b[48;5;35m"
#define BG_ALLOC     "\x1b[48;5;39m"
#define FG_FREE      "\x1b[38;5;35m"
#define FG_ALLOC     "\x1b[38;5;39m"
#define CL_RESET     "\x1b[0m"


typedef struct point {int x, y;} point;

static point offset_to_point(int offset,  int size, int is_end);
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20],
                        int offset, char * label);



// Print fancy 2D view of memory
// Note, This is limited to memory_sizes of under 16MB
void vlad_reveal(void *alpha[26])
{
    int i, j;
    vlink_t offset;
    char graph[STAT_HEIGHT][STAT_WIDTH][20];
    char free_sizes[26][32];
    char alloc_sizes[26][32];
    char label[3]; // letters for used memory, numbers for free memory
    int free_count, alloc_count, max_count;
    free_header_t * block;

	// TODO
	// REMOVE these statements when your vlad_malloc() is done
  //  printf("vlad_reveal() won't work until vlad_malloc() works\n");
    //return;

    // initilise size lists
    for (i=0; i<26; i++) {
        free_sizes[i][0]= '\0';
        alloc_sizes[i][0]= '\0';
    }

    // Fill graph with free memory
    offset = 0;
    i = 1;
    free_count = 0;
    while (offset < memory_size){
        block = (free_header_t *)(memory + offset);
        if (block->magic == MAGIC_FREE) {
            snprintf(free_sizes[free_count++], 32,
                "%d) %d bytes", i, block->size);
            snprintf(label, 3, "%d", i++);
            fill_block(graph, offset,label);
        }
        offset += block->size;
    }

    // Fill graph with allocated memory
    alloc_count = 0;
    for (i=0; i<26; i++) {
        if (alpha[i] != NULL) {
            offset = ((byte *) alpha[i] - (byte *) memory) - HEADER_SIZE;
            block = (free_header_t *)(memory + offset);
            snprintf(alloc_sizes[alloc_count++], 32,
                "%c) %d bytes", 'a' + i, block->size);
            snprintf(label, 3, "%c", 'a' + i);
            fill_block(graph, offset,label);
        }
    }

    // Print all the memory!
    for (i=0; i<STAT_HEIGHT; i++) {
        for (j=0; j<STAT_WIDTH; j++) {
            printf("%s", graph[i][j]);
        }
        printf("\n");
    }

    //Print table of sizes
    max_count = (free_count > alloc_count)? free_count: alloc_count;
    printf(FG_FREE"%-32s"CL_RESET, "Free");
    if (alloc_count > 0){
        printf(FG_ALLOC"%s\n"CL_RESET, "Allocated");
    } else {
        printf("\n");
    }
    for (i=0; i<max_count;i++) {
        printf("%-32s%s\n", free_sizes[i], alloc_sizes[i]);
    }

}

// Fill block area
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20],
                        int offset, char * label)
{
    point start, end;
    free_header_t * block;
    char * color;
    char text[3];
    block = (free_header_t *)(memory + offset);
    start = offset_to_point(offset, memory_size, 0);
    end = offset_to_point(offset + block->size, memory_size, 1);
    color = (block->magic == MAGIC_FREE) ? BG_FREE: BG_ALLOC;

    int x, y;
    for (y=start.y; y < end.y; y++) {
        for (x=start.x; x < end.x; x++) {
            if (x == start.x && y == start.y) {
                // draw top left corner
                snprintf(text, 3, "|%s", label);
            } else if (x == start.x && y == end.y - 1) {
                // draw bottom left corner
                snprintf(text, 3, "|_");
            } else if (y == end.y - 1) {
                // draw bottom border
                snprintf(text, 3, "__");
            } else if (x == start.x) {
                // draw left border
                snprintf(text, 3, "| ");
            } else {
                snprintf(text, 3, "  ");
            }
            sprintf(graph[y][x], "%s%s"CL_RESET, color, text);
        }
    }
}

// Converts offset to coordinate
static point offset_to_point(int offset,  int size, int is_end)
{
    int pot[2] = {STAT_WIDTH, STAT_HEIGHT}; // potential XY
    int crd[2] = {0};                       // coordinates
    int sign = 1;                           // Adding/Subtracting
    int inY = 0;                            // which axis context
    int curr = size >> 1;                   // first bit to check
    point c;                                // final coordinate
    if (is_end) {
        offset = size - offset;
        crd[0] = STAT_WIDTH;
        crd[1] = STAT_HEIGHT;
        sign = -1;
    }
    while (curr) {
        pot[inY] >>= 1;
        if (curr & offset) {
            crd[inY] += pot[inY]*sign;
        }
        inY = !inY; // flip which axis to look at
        curr >>= 1; // shift to the right to advance
    }
    c.x = crd[0];
    c.y = crd[1];
    return c;
}
