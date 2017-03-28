
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define TLB_SIZE 16
#define PAGES 256
#define PAGE_MASK 255

#define PAGE_SIZE 256
#define OFFSET_BITS 8
#define OFFSET_MASK 255


/* I defined number of frames in page table and made necessary changes. */
#define FRAMES 64

#define MEMORY_SIZE PAGES * PAGE_SIZE

// Max number of characters per line of input file to read.
#define BUFFER_SIZE 10

struct tlbentry {
unsigned char logical;
unsigned char physical;
};

// TLB is kept track of as a circular array, with the oldest element being overwritten once the TLB is full.
struct tlbentry tlb[TLB_SIZE];
// number of inserts into TLB that have been completed. Use as tlbindex % TLB_SIZE for the index of the next TLB line to use.
int tlbindex = 0;

// pagetable[logical_page] is the physical page number for logical page. Value is -1 if that logical page isn't yet in the table.
int pagetable[FRAMES];


//Added declarations


//I hold the paging times in frametracks array.
int frametracks[FRAMES];
int timer = 0;
int ffid = 0;

//lfid is used for FIFO it increased every iteratrion.
//I take its modulo based on number of frames.
int lfid = 0;

//Additional functions


//This function checks the whether current page is in the table or not.
//If it is, it returns the page index.
int searchTable(int addr, int frs[]){
  int i,index=-1;

  for(i=0;i<FRAMES;i++){
    if(frs[i]==addr)
      index = i;
  }

  return index;
}

//This function looks for free frame in the page table.
int searchFreeFrame(){
  int i,r=-1;
  for(i=0;i<FRAMES;i++){
    if(frametracks[i]==-1){
      r = i;
      break;
    }
  }
  return r;
}


//This function returns the index of the Least Recently Used page.
int findLRU(){
  int i,index,min=0;
  for(i=0;i<FRAMES;i++){

    if(frametracks[i]<min){
      min = frametracks[i];
      index= i;
    }

  }

  return index;

}




signed char main_memory[MEMORY_SIZE];

// Pointer to memory mapped backing file
signed char *backing;

int max(int a, int b)
{
if (a > b)
  return a;
return b;
}

/* Returns the physical address from TLB or -1 if not present. */
int search_tlb(unsigned char logical_page) {
int i;
for (i = max((tlbindex - TLB_SIZE), 0); i < tlbindex; i++) {
  struct tlbentry *entry = &tlb[i % TLB_SIZE];
  
  if (entry->logical == logical_page) {
    return entry->physical;
  }
}

return -1;
}

/* Adds the specified mapping to the TLB, replacing the oldest mapping (FIFO replacement). */
void add_to_tlb(unsigned char logical, unsigned char physical) {
struct tlbentry *entry = &tlb[tlbindex % TLB_SIZE];
tlbindex++;
entry->logical = logical;
entry->physical = physical;
}

int main(int argc, const char *argv[])
{

if (argc != 5) {
  fprintf(stderr, "Usage ./virtmem backingstore input -p type_number\n");
  exit(1);
}



int replacement_type = atoi(argv[4]);


const char *backing_filename = argv[1]; 
int backing_fd = open(backing_filename, O_RDONLY);
backing = mmap(0, MEMORY_SIZE, PROT_READ, MAP_PRIVATE, backing_fd, 0); 

const char *input_filename = argv[2];
FILE *input_fp = fopen(input_filename, "r");

// Fill page table entries with -1 for initially empty table.
int i;
for (i = 0; i < FRAMES; i++) {
  pagetable[i] = -1;
  frametracks[i] = -1;
}

// Character buffer for reading lines of input file.
char buffer[BUFFER_SIZE];

// Data we need to keep track of to compute stats at end.
int total_addresses = 0;
int tlb_hits = 0;
int page_faults = 0;

// Number of the next unallocated physical page in main memory
unsigned char free_page = 0;

while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
  total_addresses++;
  timer++;
  int logical_address = atoi(buffer);
  int offset = logical_address & OFFSET_MASK;
  int logical_page = (logical_address >> OFFSET_BITS) & PAGE_MASK;
  
  int physical_page = search_tlb(logical_page);
  // TLB hit
  if (physical_page != -1) {
    tlb_hits++;
    // TLB miss
  } else {


    physical_page = searchTable(logical_page,pagetable);
    
    // Page fault
    if (physical_page == -1) {
    	page_faults++;


      //Looks for a free frame in page table.
      ffid = searchFreeFrame();      


      if(ffid == -1){
        //FIFO
        if(replacement_type==0){

          //The page table is loaded from 0 to FRAMES-1
          //After the table is full, we turn back to zero.
          //FIFO is implemented.
          physical_page = lfid % FRAMES;
          lfid++;
          

        } 
        //LRU
        else if(replacement_type==1){

          //findURL looks for the least recently used frame in page table by
          //looking through frametracker array. It is used both to track free frames and
          //use times of pages.
          physical_page = findLRU();

        }

      } else {
        printf("Free page is loaded: ");
        //If there is a free page
        physical_page = ffid;

      }
    	
    	
    	// Copy page from backing file into physical memory
    	memcpy(main_memory + physical_page * PAGE_SIZE, backing + logical_page * PAGE_SIZE, PAGE_SIZE);
    	
    	pagetable[physical_page] = logical_page;



    } else{
      //updates



      printf("Found in the table: ");

    }

    //Here the time in frametracke array is updated.
    frametracks[physical_page] = timer;
    add_to_tlb(logical_page, physical_page);
  }

  
  
  int physical_address = (physical_page << OFFSET_BITS) | offset;
  signed char value = main_memory[physical_page * PAGE_SIZE + offset];
  
  printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, physical_address, value);
}

printf("Number of Translated Addresses = %d\n", total_addresses);
printf("Page Faults = %d\n", page_faults);
printf("Page Fault Rate = %.3f\n", page_faults / (1. * total_addresses));
printf("TLB Hits = %d\n", tlb_hits);
printf("TLB Hit Rate = %.3f\n", tlb_hits / (1. * total_addresses));

return 0;
}
