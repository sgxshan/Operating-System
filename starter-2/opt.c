#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"
#include "sim.h"

//extern int memsize;

extern int debug;

extern struct frame *coremap;

/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

typedef struct llnode_t
{
    //to identify the same page
    addr_t vaddr;
    //point to next node
    struct llnode_t* next;
    
}llnode;

llnode* opt_head;
llnode* opt_current;
int* opt_distance;
int count;

//our strategy is to traverse the list from current node, calculate the distance of each
//physical memory frame, choose the frame that have longest distance which means it will not
//be used in longest time

int opt_evict() {
    
    int longest_distance=-1;
    int longest_frame=-1;
    int i;    
    for (i=0; i<memsize; i++) {
        addr_t target_addr=coremap[i].vaddr;
        llnode *p=(llnode*)malloc(sizeof(llnode));
        p=opt_current;
        int distance=0;
        //traverse the physical memory to calculate its' distance
        while (p&&p->vaddr!=target_addr) {
            p=p->next;
            distance++;
        }
        //if there is another p in later trace
        if (p) {
            opt_distance[i]=distance;
        }
        //if p never appear in later trace
        else
        {
            return i;
        }
        
        if (distance>longest_distance) {
            longest_distance=distance;
            longest_frame=i;
        }
    }
    
	return longest_frame;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
//cause we know all the trace, when a p comes in, we simply move the linklist to the next node
void opt_ref(pgtbl_entry_t *p) {

    count++;
    opt_current=opt_current->next;
    
    if (opt_current==NULL) {
        opt_current=opt_head;
    }
	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
//when initialized, read all the trace from tracefile to later use
void opt_init() {

    FILE* fp=fopen(tracefile,"r");
    char buf[MAXLINE];
    addr_t vaddr;
    char type;
    
    opt_head=NULL;
    opt_current=NULL;
    
    while (fgets(buf,MAXLINE,fp)!=NULL) {
        if(buf[0]!='=')
        {
            sscanf(buf,"%c %lx",&type,&vaddr);
            
            llnode* new_node=(llnode*)malloc(sizeof(llnode));
            //cause we do not need offset to choose the frame
            new_node->vaddr=(vaddr>>PAGE_SHIFT)<<PAGE_SHIFT;
            new_node->next=NULL;
            
            if(opt_head==NULL)
            {
                opt_head=new_node;
                opt_current=new_node;
            }
            else
            {
                opt_current->next=new_node;
                opt_current=new_node;
            }
        }
    }
    
    opt_current=opt_head;
    opt_distance=malloc(sizeof(int)*memsize);
    count=0;
}

