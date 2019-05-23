#include <stdio.h>  // needed for size_t
#include <sys/mman.h> // needed for mmap
#include <assert.h> // needed for asserts
#include "dmm.h"

/* You can improve the below metadata structure using the concepts from Bryant
 * and OHallaron book (chapter 9).
 */

typedef struct metadata {
  /* size_t is the return type of the sizeof operator. Since the size of an
   * object depends on the architecture and its implementation, size_t is used
   * to represent the maximum size of any object in the particular
   * implementation. size contains the size of the data object or the number of
   * free bytes
   */

  size_t size;
  struct metadata* next;
  struct metadata* prev; 
} metadata_t;


typedef struct {
    bool prevAlloc, nextAlloc;
} tuple;

/* freelist maintains all the blocks which are not in use; freelist is kept
 * sorted to improve coalescing efficiency 
 */

static metadata_t* freelist = NULL;


//Search for the FIRST block that is big enough to accommodate the dmalloc request
static metadata_t* searchFirstFit(size_t size){
    //Define a local instance of the head of our FreeList
    metadata_t* FL_Head = freelist;
    size_t sizeNeeded = size + ALIGN(1);
    //Loop through every block in our free list until we find one big enough for the request size AND the header size
    while((FL_Head->next != NULL) && (FL_Head->size >0)){
        printf("%zu", FL_Head->size);
        if(FL_Head->size >= sizeNeeded) {
            return FL_Head;
        }
        // If the current block didn't satisfy the requirements, go to the next one
        FL_Head = FL_Head->next;
    }

    return NULL;
}

void nullify(metadata_t* block){
    block->prev = NULL;
    block->next = NULL;
}

//Helper method to see if a block is allocated or not
bool isPrevAllocated(metadata_t* ptr){
    if (ptr == NULL){
        return false;
    }
    bool isAllocated = (bool) (ptr->size & 1);
    if(ptr->size == 0) {
        freelist = ptr;
        isAllocated = true;
        
    }
    return isAllocated;
}

//Helper method to see if a block is allocated or not
bool isNextAllocated(metadata_t* ptr){
    if (ptr == NULL){
        return false;
    }
    bool isAllocated = (bool) (ptr->size & 1);
    if(ptr->size == 0) {
        isAllocated = true;
        
    }
    return isAllocated;
}

//Sets up the allocations of the next an previous blocks of the block we want to free
tuple setUpAllocations(metadata_t* headptr, metadata_t* prevptr, metadata_t* nextptr){
    assert(prevAllocated);
    assert(nextAllocated);
    //Set up a temporary pointer that points at the current block
    void* tmp = ((void*) prevptr); 
    tmp = tmp + prevptr->size + METADATA_T_ALIGNED;

    //If tmp is not the same as the head pointer, then it is NOT allocated 
    bool prevAllocated = ((metadata_t*) tmp) != headptr; 

    //Now make tmp point at the next pointer
    tmp = ((void*) headptr) + headptr->size + METADATA_T_ALIGNED;

    //if tmp is not the same as nextptr, then it is NOT allocated either.
    bool nextAllocated = ((metadata_t*) tmp) != nextptr;

    if(nextptr->size == 0) {
        nextAllocated= true;
    }

    //Now, if the size of the previous pointer is 0, it IS allocated. ONLY happens when it is the PROLOGUE
    if(prevptr->size == 0) {
      //AKA prevpt is the prologue
        prevAllocated = true;
        //Freelist should now be the same as headptr
        freelist = headptr;
    }

    tuple allocations = {prevAllocated, nextAllocated};
    return allocations;
  
}

/**
 * the main splitting method. Occurs when the block we found through our fit algorithm is larger than the
 * size of the request, plus the size of the metadata header
 * @param foundBlock
 * @param size
 */

void dsplit(metadata_t* foundBlock, size_t size, size_t remaining){
    metadata_t* nblock = ((metadata_t*) (foundBlock + 1)); //create a pointer exactly at the foundblock
    void* temp = ((void*) nblock) + size; //Move the pointer 'size' units.
    //Create a new block, that will be the "split" block
    nblock = (metadata_t*) temp; //create a temporary 'nblock' that points to temp

    //ASSUMPTION: If the original block was size X and size Y was requested, the first Y memory of X was first. 
    //Therefore, the memory that is left will be on the right. 
    foundBlock->prev->next = nblock;
    foundBlock->next->prev = nblock;

    //Set the next and prev of foundBlock to nblock
    nblock->size = remaining - METADATA_T_ALIGNED;
    nblock->prev = foundBlock->prev;
    nblock->next = foundBlock->next; //Set the size of nblock to the remaining size of the 'split'

    
    //Important, occurs when the freelist is the same as the foundblock (EDGE CASE)
    if(foundBlock == freelist){
      freelist = nblock;
        
    }
    //Nullify pointer
    nullify(foundBlock);
    foundBlock->size = size;

}

/**
* Removes a block from the freeList without the need to split
*/

void removeFromFreeList(metadata_t* block){
    block->prev->next = block->next;
    block->next->prev = block->prev;
    nullify(block);
        
}

void addToFreelist(metadata_t* currHead, metadata_t* prevptr, metadata_t* nextptr){
    //In this instance, neither the next or previous blocks were free, so we must add our
    //recently freed block to our freelist
    //All it takes is some linking.
    currHead->prev = prevptr;
    currHead->next = nextptr;
    prevptr->next = currHead;
    nextptr->prev = currHead;
}

void mergePrevToCurrent(metadata_t* currHead, metadata_t* prevptr, metadata_t* nextptr){
    //This one is the easiest. When we need to merge ONLY the previous pointer with our recently freed block,
    //All we have to do is increase the size of the previous pointer to encapsulate the recently freed block
    size_t sizeToAdd = currHead->size + METADATA_T_ALIGNED;
    prevptr->size += sizeToAdd;
}


void mergeNextToCurrent(metadata_t* currHead, metadata_t* prevptr, metadata_t* nextptr){
    //This is similar to above. we essentially want to add up the sizes of the recently freed block
    //and the 'next' block in the freelist.
    size_t sizeToAdd = nextptr->size + METADATA_T_ALIGNED;
    currHead->size += sizeToAdd;
    //fix some links of currHead to get rid of nextptr
    currHead->prev = prevptr;
    currHead->next = nextptr->next;
    prevptr->next = currHead;
    currHead->next->prev = currHead;
}

//Our coalescence method. Allows us to combine two adjacent blocks of memory in the heap that have been freed.
void coalescence(bool prevAllocated, bool nextAllocated, metadata_t* headptr, metadata_t* prevptr, metadata_t* nextptr) {


    //if both already allocated, return and add main freed block to freelist
    printf("made it this far is coalescence\n");
    if (prevAllocated && nextAllocated){
        addToFreelist(headptr, prevptr, nextptr);
        return;
    }
    else if (!prevAllocated && nextAllocated){
        mergePrevToCurrent(headptr, prevptr, nextptr);
    }
    else if(prevAllocated && !nextAllocated){
        mergeNextToCurrent(headptr, prevptr, nextptr);
    }
    else{
      //add up the current pointer size, the next one, and the two headers
        prevptr->size += headptr->size + nextptr->size + 2*METADATA_T_ALIGNED;
        //set the next of previous pointer to next of next pointer
        prevptr->next = nextptr->next;
        //finish up the links
        nextptr->next->prev = prevptr;
    }

}


/**
 * //Places the block of memory that we found in the call to the method searchFirstFit. One decision is whether I check if the block
//is big enough to also contain the MetaData here, or in the searchFirstFit method. For now, I will do it in the searchFirstFit method.
 * @param foundBlock
 * @param size
 */
void placeBlock(metadata_t* foundBlock, size_t size) {
    //Obtain the amount of the block that remains
    size_t remaining = foundBlock->size - size;
    //If the size of the foundBlock is greater than the requested size, we need to SPLIT it
    if(remaining >= METADATA_T_ALIGNED + ALIGN(1)){
        dsplit(foundBlock, size, remaining);
    }


    //If there isn't enough extra memory to split, don't split
    else{
        printf("Not enough space to split (aka not enough space for new header\n");
        if(freelist == foundBlock){
            freelist = foundBlock->next;
        }
        removeFromFreeList(foundBlock);
    }
}



/**
 * my implementation of the malloc API method. Returns a pointer to a chunk of memory of the requested size
 *
 * @param numbytes
 * @return
 */

void* dmalloc(size_t numbytes) {
  /* initialize through mmap call first time */
  if(freelist == NULL) {            
    if(!dmalloc_init())
      return NULL;
  }
  assert(numbytes > 0); //error handling to assure user isn't requesting 0 bytes
  //Logic for malloc: User will call dmalloc(size), and this API will return a pointer to a chunk of memory that they can use.
  //This chunk of memory is a block from our heap, which right now should be of size 1024 bytes.

  size_t sizeAlign = ALIGN(numbytes);
  //Now, we can find the first fit using out method
  metadata_t* firstFit = searchFirstFit(sizeAlign);

  //If fit is null, return a NULL memory block
  if(firstFit == NULL){
      printf("Null is being returned\n");
      return NULL;
  }

  //otherwise, we have to update out freeList, and then return the block AFTER adding a metadata header
  printf("Fit was successfully found\n");
  placeBlock(firstFit, sizeAlign);
  firstFit = (metadata_t*) (firstFit + 1);
  return firstFit;
}

/**
 *
 *When free is called, our API has to firstly locate the memory address in the heap, obtain the block, and add it to the free list.
 * Afterwards, we can call coalescence on the entire freelist to patch up any adjacent blocks that are freed. Calling coalescence after every
 * free may be inefficient, but it the best way to avoid fragmentation.
 * @param ptr
 */
//TODO: REFACTOR
void dfree(void* freedptr) {
    //Since I want to keep the freeList ordered, I have to find the previous pointer of my freeList, the next one, and then set
    // the head pointer of my freeList to the start of the metadata of the pointer I want to free
    metadata_t* prevptr = freelist->prev;
    metadata_t* nextptr = freelist;
    //The head of the freeList becomes the start of the freed block
    metadata_t* headptr = (metadata_t*) (freedptr-METADATA_T_ALIGNED);
    //While we are here, we can go ahead and run through the freelist to coalesce any adjacent freed blocks
    while(nextptr != NULL){
        if(prevptr < headptr){
          if(headptr < nextptr){
            //get our allocations first
            printf("triggered\n");
              tuple allocations = setUpAllocations(headptr, prevptr, nextptr);
              bool prevAllocated = allocations.prevAlloc;
              bool nextAllocated = allocations.nextAlloc;
            
              coalescence(prevAllocated, nextAllocated, headptr, prevptr, nextptr);
              break;
          }
        } 
        prevptr = prevptr->next;
        nextptr = nextptr->next;
    }
}

bool dmalloc_init() {

  /* Two choices: 
   * 1. Append myPrologue and myEpilogue blocks to the start and the
   * end of the freelist 
   *
   * 2. Initialize freelist pointers to NULL
   *
   * Note: We provide the code for 2. Using 1 will help you to tackle the 
   * corner cases succinctly.
   */

  size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
  ///* returns heap_region, which is initialized to freelist */
  //Going to do the myPrologue / myEpilogue method
  //myEpilogue will be the pointer at the start of the myEpilogue, essentially
  metadata_t* starting_heap = (metadata_t*) mmap(NULL, max_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    /* Q: Why casting is used? i.e., why (void*)-1? */
    if(starting_heap == (void*) - 1) {
        return false;
    }

  metadata_t* myEpilogue = starting_heap;
  /* create myPrologue and shift position in memory of freelist */
  myEpilogue->size = max_bytes - METADATA_T_ALIGNED;
  nullify(myEpilogue);

  metadata_t* myPrologue = myEpilogue;
  myEpilogue = (metadata_t*) (myEpilogue + 1);
  myPrologue->prev = NULL;
  myPrologue->next = myEpilogue;
  myEpilogue->prev = myPrologue;

  //Set the size of both the prologue and epilogue.
  myEpilogue->size = max_bytes - 2*METADATA_T_ALIGNED;
  myPrologue->size = 0;
  
  //Set the head of our freelist to myEpilogue
  freelist = myEpilogue;
  void* tmp = ((void*) myEpilogue) + myEpilogue->size;
  myEpilogue = (metadata_t*) tmp;

  //Set up the freeList using epilogue and prologue. Essentially sandwhich it between the two.
  freelist->prev = myPrologue;
  freelist->next = myEpilogue;
  myEpilogue->prev = freelist;
  freelist->size = max_bytes - 3*METADATA_T_ALIGNED;

  //Lastly, set the size of myEpilogue to 0
  myEpilogue->size = 0;
  return true;

}

/* for debugging; can be turned off through -NDEBUG flag*/
void print_freelist() {
  metadata_t *freelist_head = freelist;
  while(freelist_head != NULL) {
    DEBUG("\tFreelist Size:%zd, Head:%p, Prev:%p, Next:%p\t",
      freelist_head->size,
      freelist_head,
      freelist_head->prev,
      freelist_head->next);
    freelist_head = freelist_head->next;
  }
  DEBUG("\n");
}
