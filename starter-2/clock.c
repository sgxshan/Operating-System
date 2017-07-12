#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

bool* clock_refed;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_pointer;

int clock_evict() {
    //when the frame refered as 1, change it to 0 and move to next frame
	while(clock_refed[clock_pointer]==1)
    {
        clock_refed[clock_pointer]=0;
        clock_pointer=(clock_pointer+1)%memsize;
    }
    return clock_pointer;
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {

    //shift to remove offset cause we only need frame number to mark the page
    clock_refed[p->frame>>PAGE_SHIFT]=1;
	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {
    clock_pointer=0;
    clock_refed=malloc(sizeof(bool)*memsize);
    memset(clock_refed, 0, sizeof(bool) * memsize);
}
