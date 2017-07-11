#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdbool.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

typedef struct llnode_t
{
    //to store frame number
    int frame;
    //point to next lnode
    struct llnode_t* next;
    
}llnode;

//use list to store past sequence of frame, head is the least recent use frame,and tail is the recent used frame
llnode* lru_head;
llnode* lru_tail;
bool* lru_refed;

//remove the head of the list
int lru_evict() {
    int frame=lru_head->frame;
    llnode* new_head=lru_head->next;
    free(lru_head);
    lru_head=new_head;
    lru_refed[frame]=0;
	return frame;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
//in lru algorithm, when a page is referenced, we move/record this page to the tail of the list
void lru_ref(pgtbl_entry_t *p) {

    //do not need offset to update a page
    int frame=p->frame>>PAGE_SHIFT;
    llnode* new_node=(llnode*)malloc(sizeof(llnode));
    new_node->frame=frame;
    new_node->next=NULL;
    
    if(lru_tail==NULL)
    {
        lru_tail=new_node;
        lru_head=new_node;
    }
    else if(lru_refed[frame]==0)
    {
        lru_tail->next=new_node;
        lru_tail=new_node;
    }else
    {
        lru_tail->next=new_node;
        lru_tail=new_node;
        llnode* lru_current=(llnode*)malloc(sizeof(llnode));
        llnode* prev=(llnode*)malloc(sizeof(llnode));
        lru_current=lru_head;
        prev=NULL;
        while (lru_current->frame!=frame) {
            prev=lru_current;
//            lru_current=lru_current->next;
        }
        
        if (prev!=NULL) {
            prev->next=lru_current->next;
            free(lru_current);
//            lru_current=prev->next;
        }else
        {
            lru_head=lru_head->next;
            free(lru_current);
//            lru_current=lru_head;
        }
        
    }
    
    lru_refed[frame]=1;
	return;
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
    lru_head=NULL;
    lru_tail=NULL;
    lru_refed = malloc(sizeof(bool) * memsize);
    memset(lru_refed, 0, sizeof(bool) * memsize);
}
