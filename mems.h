/*
All the main functions with respect to the MeMS are inplemented here
read the function discription for more details

NOTE: DO NOT CHANGE THE NAME OR SIGNATURE OF FUNCTIONS ALREADY PROVIDED
you are only allowed to implement the functions 
you can also make additional helper functions a you wish

REFER DOCUMENTATION FOR MORE DETAILS ON FUNSTIONS AND THEIR FUNCTIONALITY
*/
// add other headers as required
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/*
Use this macro where ever you need PAGE_SIZE.
As PAGESIZE can differ system to system we should have flexibility to modify this 
macro to make the output of all system same and conduct a fair evaluation. 
*/

#define PAGE_SIZE 4096


typedef struct SubChainNode {
    size_t size;
    bool isHole;
    struct SubChainNode *next;
    struct SubChainNode *prev;
    void* lowerBoundAddress;
    void* upperBoundAddress;
    void* ending_virtual;
    void* starting_virtual;
    int data;
}SubChainNode;

typedef struct MainChainNode {
    SubChainNode *SubHead; 
    size_t memSize;
    struct MainChainNode *next;
    struct MainChainNode *prev;
    void* lowerBoundAddress;
    void* upperBoundAddress;
    int data;
    void* ending_virtual;
    void* starting_virtual;
}MainChainNode;


int pagesUsed;
int spaceUnused;
MainChainNode* mainHead;
void* mem_start_virtual;  
const void* mem_start_physical;


int lengthOfMainList(MainChainNode* head) {

    int length = 0;
    MainChainNode* current = head;
    while (current != NULL) {
        length++;
        current = current->next;
    }
    return length;
}

int lengthOfSubList(SubChainNode* head) {
    int length = 0;
    SubChainNode* current = head;
    while (current != NULL) {
        length++;
        current = current->next;
    }
    return length;
}

void combine(){

    MainChainNode *current = mainHead;
    while (current != NULL) {

        SubChainNode *sub_current = current->SubHead;
        while (sub_current != NULL) {

            if (sub_current->isHole == true) {

                // Checking if the previous and next nodes are holes, if yes then combine all into one node
                if (sub_current->prev != NULL && sub_current->prev->isHole) {
                    SubChainNode *prev_node = sub_current->prev;
                    prev_node->size += sub_current->size;
                    prev_node->upperBoundAddress = sub_current->upperBoundAddress;
                    prev_node->ending_virtual = sub_current->ending_virtual;
                    prev_node->next = sub_current->next;
                    if (sub_current->next != NULL) {
                        sub_current->next->prev = prev_node;
                    }
                    sub_current = prev_node;
                }
                if (sub_current->next != NULL && sub_current->next->isHole) {
                    SubChainNode *next_node = sub_current->next;
                    sub_current->size += next_node->size;
                    sub_current->upperBoundAddress = next_node->upperBoundAddress;
                    sub_current->ending_virtual = next_node->ending_virtual;
                    sub_current->next = next_node->next;
                    if (next_node->next != NULL) {
                        next_node->next->prev = sub_current;
                    }
                }
            }
            sub_current = sub_current->next;
        }
        current = current->next;
    }
}


/*
Initializes all the required parameters for the MeMS system. The main parameters to be initialized are:
1. the head of the free list i.e. the pointer that points to the head of the free list
2. the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
3. any other global variable that you want for the MeMS implementation can be initialized here.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_init(){

    
    int fd = open("/dev/zero", O_RDWR);

    mainHead = (MainChainNode *)mmap(NULL, sizeof(MainChainNode), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    mainHead->lowerBoundAddress = mainHead; 
    mainHead->upperBoundAddress = mainHead+sizeof(MainChainNode);
    mainHead->memSize = sizeof(MainChainNode);
    mainHead->next = NULL;
    mainHead->prev = NULL;
    mainHead->SubHead = NULL;
    mainHead->data = 1;
    mem_start_virtual = (void*)1000;

    pagesUsed = 0;
    // e = 0;
    mem_start_physical = mainHead->lowerBoundAddress;
}


/*
This function will be called at the end of the MeMS system and its main job is to unmap the 
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish(){

    MainChainNode *current_node = mainHead;
    int count = 0;
    while (current_node != NULL) {

        if(count!=0){

            // printf("jjjj\n");

            void* start = current_node->lowerBoundAddress;
            size_t len = current_node->upperBoundAddress - current_node->lowerBoundAddress;

            // printf("%p\n", start);
            // printf("%ld\n", len);

            // if (munmap(start, len) == -1) {
            //     perror("Error in munmap");
            //     return;
            // }

            // printf("ssss\n");
        }
        SubChainNode *currentsub = current_node->SubHead;
        while (currentsub != NULL) {
            SubChainNode *temp = currentsub;
            currentsub = currentsub->next;
            if (munmap(temp, sizeof(SubChainNode)) == -1) {
                printf("\n");
                perror("Error in munmap.");
                printf("\n");
                return;
            }
            
        }
        MainChainNode *temp_node = current_node;
        current_node = current_node->next;
        if ( munmap(temp_node, sizeof(MainChainNode)) == -1) {
            printf("\n");
            perror("Error in munmap.");
            printf("\n");
            return;
        }
        count++;

    }
    // munmap(mainHead, sizeof(MainChainNode));
    mainHead = NULL;
    // printf("\nMemory has been deallocated\n");
   
}


/*
Allocates memory of the specified size by reusing a segment from the free list if 
a sufficiently large segment is available. 

Else, uses the mmap system call to allocate more memory on the heap and updates 
the free list accordingly.

Note that while mapping using mmap do not forget to reuse the unused space from mapping
by adding it to the free list.
Parameter: The size of the memory the user program wants
Returns: MeMS Virtual address (that is created by MeMS)
*/ 


void* mems_malloc(size_t size){

    if((int)size<0){

        errno = EINVAL;
        printf("\n");
        perror("Negative or 0 memory cannot be allocated. mems_malloc().");
        printf("\n");
        return NULL;
    }

    combine();
    static void* current_address = NULL;
    int fd = open("/dev/zero", O_RDWR);

    int *allottedMemory;
    size_t requestedMemory;

    if( size <= PAGE_SIZE){

        requestedMemory = PAGE_SIZE;
    }
    else{

        requestedMemory = ((size / PAGE_SIZE)+ 1) * PAGE_SIZE;
    }

    MainChainNode* itr = mainHead;//itr is iterator which will iterate over all main chain nodes
    
    //iterating on each sublinkedlist and its nodes...
    while(itr!=NULL){
        SubChainNode* itr2 = itr->SubHead;//itr2 is iterator which iterates over all sublist nodes respectively
        while(itr2 != NULL){
            if(itr2->isHole && itr2->size >= size){
                //means hole can be used to allocate memory
                itr2->isHole = false; // i.e, making itr2 a process and remaining chunk from itr2 vali node becomes hole

                if(itr2->size == size){
                    //means hole will be just converted to process and another hole won't be created since hole's full memory is utilised.
                    return itr2->starting_virtual;
                }
                size_t leftSize = (itr2->size) - size; // this leftsize is now size of hole which we are adding below

                SubChainNode * holee = (SubChainNode *)mmap(NULL, sizeof(SubChainNode), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

                itr2->size = size;

                void* store1 = itr2->upperBoundAddress;
                void* store2 = itr2->ending_virtual;
                itr2->upperBoundAddress = itr2->lowerBoundAddress + size-1;
                itr2->ending_virtual = itr2->starting_virtual+size-1;

                holee->isHole = true;
                holee->size = leftSize;
                
                holee->lowerBoundAddress = itr2->upperBoundAddress+1;
                holee->upperBoundAddress = store1;

                holee->starting_virtual = itr2 ->ending_virtual+1;
                holee->ending_virtual = store2;

                //adding node now 
                holee->next = itr2->next;
                holee->prev = itr2;
                itr2->next = holee;
                return itr2->starting_virtual;
            }

            //handle combining holes vala case...
            itr2 = itr2->next;
        }

        itr2 = itr->SubHead;
        while (itr2 != NULL && itr2->next != NULL) {
            if (itr2->isHole && itr2->next->isHole) {
                // Merging consecutive holes
                SubChainNode *next_node = itr2->next;
                itr2->size += next_node->size;
                itr2->upperBoundAddress = next_node->upperBoundAddress;
                itr2->next = next_node->next;
                if (next_node->next != NULL) {
                    next_node->next->prev = itr2;
                }
            } else {
                itr2 = itr2->next;
            }
        }

        itr = itr->next;
    }


    //here i am bringing itr to last position of mainlist 
    if(mainHead->next == NULL){
        itr = mainHead;
    }
    else{
        itr = mainHead->next;
        while(itr->next!=NULL){
            itr = itr->next;
        }
    }


    //comes here when holes don't have enough memory which was required.
    //means new main node is to be created now .
    if (current_address == NULL) {
        current_address = mmap(NULL, requestedMemory, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (current_address == MAP_FAILED) {
            perror("Memory allocation failed. mems_malloc error.\n");
            return NULL;
        }
        mem_start_physical = current_address;

    } else {

        current_address = (void*)((char*)current_address + requestedMemory);
    }

    allottedMemory = current_address;

    size_t numOfpages = ((size / PAGE_SIZE)+ 1);
    pagesUsed += numOfpages;

    //now creating new Main chain node and initializing it
    
    MainChainNode* NODE = (MainChainNode *)mmap(NULL, sizeof(MainChainNode), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

    //eg MAIN[1000:5095], so here lowerboundAddress is 1000 and upperboundAddress is 5095
    NODE->lowerBoundAddress = allottedMemory;
    NODE->upperBoundAddress = allottedMemory + requestedMemory -1;

    NODE->starting_virtual = mem_start_virtual;
    NODE->ending_virtual = mem_start_virtual + requestedMemory - 1;
    mem_start_virtual = mem_start_virtual + requestedMemory;
    // NODE->memSize = ((size / PAGE_SIZE)+ 1);

    NODE->prev = itr;
    NODE->next = NULL;
    NODE->data = itr->data+1;
    itr->next = NODE; 

    SubChainNode* subnode = (SubChainNode *)mmap(NULL, sizeof(SubChainNode), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

    NODE->SubHead = subnode;
    subnode->isHole = false;
    subnode->next = NULL;
    subnode->prev = NULL;
    subnode->size = size;
    subnode->lowerBoundAddress = allottedMemory;
    subnode->upperBoundAddress = allottedMemory + size -1;

    subnode->starting_virtual = NODE->starting_virtual;
    subnode->ending_virtual = NODE->starting_virtual + size-1;
    

    //checking memory left if yes then create hole..
    if(PAGE_SIZE*numOfpages > size){
        size_t leftSize = (PAGE_SIZE*numOfpages) - size;
        //now create hole
        SubChainNode * holee = (SubChainNode *)mmap(NULL, sizeof(SubChainNode), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

        holee->isHole = true;
        holee->size = leftSize;
        holee->lowerBoundAddress = subnode->upperBoundAddress + 1;
        holee->upperBoundAddress = holee->lowerBoundAddress + leftSize;

        holee->starting_virtual = subnode->ending_virtual + 1;
        holee->ending_virtual = NODE->ending_virtual;

        //adding node now 
        holee->next = NULL;
        holee->prev = subnode;
        subnode->next = holee;
        holee->data = 4;
    }

    //doing mapping of virtual address with physical address.
    // virtAddr[e] = (void*)&virtualAddressValue;
    // phyAddr[e] = allottedMemory;
    // virtualAddressValue += ((numOfpages*PAGE_SIZE));
    // e++;

    return subnode->starting_virtual;
}


/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/
void mems_print_stats(){

    combine();
    printf("\n");
    printf("----MEMS SYSTEM STATS----\n");
    if(mainHead->next == NULL){
        errno = ENODATA;
        perror("No memory has been allocated until now");
        printf("\n");
        return;
    }
    MainChainNode* itr = mainHead->next;//itr is iterator which will iterate over all main chain nodes

    while(itr!=NULL){
        
        SubChainNode* itr2 = itr->SubHead;//itr2 is iterator which iterates over all sublist nodes respectively
        printf("MAIN[%lld:%lld]-> ",(long long)itr->starting_virtual,(long long)itr->ending_virtual);
    
        while(itr2!=NULL){
            if(itr2->isHole){
                printf("H[%lld:%lld] <-> ",(long long)itr2->starting_virtual,(long long)itr2->ending_virtual);
            }
            else{
                printf("P[%lld:%lld] <-> ",(long long)itr2->starting_virtual,(long long)itr2->ending_virtual);
            }
            itr2 = itr2->next;
        }
        if(itr2 == NULL){
            printf("NULL\n");   
        }
        itr = itr->next;
    }
    
    printf("Pages used: %d\n", pagesUsed);

    MainChainNode* itr_first1 = mainHead->next;
    while(itr_first1!=NULL){
        SubChainNode* itr_second1 = itr_first1->SubHead;
        while(itr_second1 != NULL){
            if(itr_second1->isHole){
                spaceUnused += itr_second1->size;
            }
            itr_second1=itr_second1->next;
        }
        itr_first1 = itr_first1->next;
    }
    printf("Space unused: %d\n", spaceUnused);
    spaceUnused = 0;

    printf("Main Chain Length: %d\n", lengthOfMainList(mainHead->next));
    printf("Sub-chain Length array : [");
    MainChainNode* itr_first = mainHead->next;
    while(itr_first!=NULL){
        SubChainNode* itr_second = itr_first->SubHead;
        printf("%d, ", lengthOfSubList(itr_second));
        itr_first = itr_first->next;
    }
    printf("]\n");
    printf("----------------------\n\n");

}


/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void *mems_get(void*v_ptr){

    void *s_ptr = NULL;
    MainChainNode *current = mainHead;
    while (current != NULL) {

        SubChainNode *sub_current = current->SubHead;
        while (sub_current != NULL) {
            
            if (v_ptr >= sub_current->starting_virtual && v_ptr < sub_current->upperBoundAddress) {
                
                size_t offset = (char *)v_ptr - (char *)1000 ;
               
                s_ptr = current->lowerBoundAddress + offset;
                return s_ptr;
            }
            sub_current = sub_current->next;
        }
        current = current->next;
    }

    if(s_ptr == NULL){

        errno = EINVAL;
        printf("\n");
        perror("Invalid pointer argument. mems_get().");
        printf("\n");
        return NULL;
    }
    return NULL;
    
}

/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS) 
Returns: nothing
*/
void mems_free(void *v_ptr){

    MainChainNode *current = mainHead;
    while (current != NULL) {

        SubChainNode *sub_current = current->SubHead;
        while (sub_current != NULL) {

            if (v_ptr == sub_current->starting_virtual) {

                sub_current->isHole = true;

                // Checking if the previous and next nodes are holes, if yes then combine all into one node
                if (sub_current->prev != NULL && sub_current->prev->isHole) {
                    SubChainNode *prev_node = sub_current->prev;
                    prev_node->size += sub_current->size;
                    prev_node->upperBoundAddress = sub_current->upperBoundAddress;
                    prev_node->ending_virtual = sub_current->ending_virtual;
                    prev_node->next = sub_current->next;
                    if (sub_current->next != NULL) {
                        sub_current->next->prev = prev_node;
                    }
                    sub_current = prev_node;
                }
                if (sub_current->next != NULL && sub_current->next->isHole) {
                    SubChainNode *next_node = sub_current->next;
                    sub_current->size += next_node->size;
                    sub_current->upperBoundAddress = next_node->upperBoundAddress;
                    sub_current->ending_virtual = next_node->ending_virtual;
                    sub_current->next = next_node->next;
                    if (next_node->next != NULL) {
                        next_node->next->prev = sub_current;
                    }
                }
                return;
            }
            sub_current = sub_current->next;
        }
        current = current->next;
    }

    errno = EINVAL;
    printf("\n");
    perror("Invalid pointer argument. mems_free().");
    printf("\n");
    return;

}
