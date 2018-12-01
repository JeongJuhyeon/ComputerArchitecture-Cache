#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define MEMORY_SIZE 64*1024*1024    // addresses are 26 bits (4 bytes)
typedef unsigned int word;

void read_config(char *file_name);
void print_config();
void process_without_cache();
void allocate_memory();
void allocate_cache();
void process_with_cache();
void place_in_back_of_queue(int block_number);
void read_word(unsigned int address);
int evict_LRU(unsigned int set_number, unsigned int address);
void overwrite_evicted_block(int evicted_block, unsigned int address, unsigned int tag);
bool place_in_free_space(unsigned int index, unsigned int address, unsigned int tag);
bool is_hit(int set_number, int tag);
void move_queue_forwards(int block_number, int after_here);
unsigned int get_index(unsigned int address);
unsigned int get_tag(unsigned int address);
unsigned int get_block_address(unsigned int index, unsigned int tag);


int ram_access = 0;

typedef struct settings{
    float CLOCK_RATE;
    int ACCESS_LATENCY;
    int CACHE_SIZE;
    int BLOCK_BYTES;
    int BLOCK_WORDS;
    int BLOCKS_PER_SET; // == Number of blocks per set
    int CACHE_BLOCKS;
    int CACHE_SETS;
    int INDEX_BITS;
    int OFFSET_BITS;
    int QUEUE_POSITION_BYTES;
}struct_settings;

float CLOCK_RATE;
int ACCESS_LATENCY;
int CACHE_SIZE;
int BLOCK_BYTES;
int BLOCK_WORDS;
int BLOCKS_PER_SET; // == Number of blocks per set
int CACHE_BLOCKS;
int CACHE_SETS;
int INDEX_BITS;
int OFFSET_BITS;
int QUEUE_POSITION_BYTES;

typedef struct block{
    bool occupied;      // == valid
    unsigned int tag;            // at most 24 bits, fits in a 32-bit int
    unsigned int queue_order;    // at most 26 bits, fits in a 32-bit int
    word *data;          // block size is 1, 2, 4, .. words (ints)
}block;

struct_settings *settings = NULL;
block *Ram;
block *Cache;

int main(int argc, char *argv[]) {
    settings = malloc(sizeof(struct settings));

    //TODO: CHANGE THIS TO argv[1] WHEN SUBMITTING
    read_config("cachesim.conf");
    print_config();

    allocate_memory();
    process_without_cache();

    allocate_cache();
    process_with_cache();




    return 0;
}

void allocate_memory(){
    Ram = calloc(sizeof(struct block), MEMORY_SIZE / BLOCK_BYTES);

    for (long int i = 0; i < MEMORY_SIZE / BLOCK_BYTES; ++i)
    {
        Ram[i].data = calloc(sizeof(word), BLOCK_WORDS);
    }
}


void allocate_cache(){
    Cache = calloc(sizeof(struct block), CACHE_BLOCKS);

    for (long int i = 0; i < CACHE_SIZE / CACHE_BLOCKS; ++i)
    {
        Cache[i].data = calloc(sizeof(word), BLOCK_WORDS);
        Cache[i].queue_order = 1;
    }
}

void place_in_back_of_queue(int block_number){
    Cache[block_number].queue_order = BLOCKS_PER_SET;
}

void process_with_cache(){

}

void read_word(unsigned int address) {
    int index = get_index(address);   // = set number
    int tag = get_tag(address);
    int evicted_block;

    // Now we have to look inside the set whether it's a hit
    bool hit = is_hit(index, tag);

    // If its a hit, theres no need to access the memory.
    if (hit) {
        return;
    }
    // If its a miss
    else if (!hit){
        // If there's a free space in the set
        if (place_in_free_space(index, address, tag))
            ram_access += 1;
        else {
            // If there isn't
            evicted_block = evict_LRU(index, address);
            move_queue_forwards(index, 1);
            overwrite_evicted_block(evicted_block, address, tag);
            ram_access += 2;
        }
    }
}


// Search for the first block in the queue, then write it into the memory
int evict_LRU(unsigned int set_number, unsigned int address){
    int first_block_in_set = set_number * BLOCKS_PER_SET;
    int ram_block_number = address / BLOCK_BYTES;
    int block_to_evict;

    for (int i = 0; i < BLOCKS_PER_SET; i++){
        if (Cache[first_block_in_set + i].queue_order == 1) {
            block_to_evict = first_block_in_set + i;
            break;
        }
    }
    memcpy(Ram[ram_block_number].data, Cache[block_to_evict].data, BLOCK_BYTES);
    return block_to_evict;
}

void overwrite_evicted_block(int evicted_block, unsigned int address, unsigned int tag){
    Cache[evicted_block].queue_order = BLOCKS_PER_SET;
    int ram_block_number = address / BLOCK_BYTES;
    memcpy(Cache[evicted_block].data, Ram[ram_block_number].data, BLOCK_BYTES);
    Cache[evicted_block].tag = tag;
}

// Try to place the read data in an unoccupied slot
bool place_in_free_space(unsigned int set_number, unsigned int address, unsigned int tag) {
    int first_block_in_set = set_number * BLOCKS_PER_SET;
    int ram_block_number = address / BLOCK_BYTES;
    for (int i = 0; i < BLOCKS_PER_SET; ++i) {
        if (!Cache[first_block_in_set + i].occupied) {
            Cache[first_block_in_set + i].occupied = true;
            Cache[first_block_in_set + i].tag = tag;
            memcpy(Cache[first_block_in_set + i].data, Ram[ram_block_number].data, BLOCK_BYTES);
            return true;
        }
    }
    return false;
}

bool is_hit(int set_number, int tag) {
    int first_block_in_set = set_number * BLOCKS_PER_SET;
    for (int i = 0; i < BLOCKS_PER_SET; ++i) {
        if (Cache[first_block_in_set + i].occupied && Cache[first_block_in_set + i].tag == tag)
            return true;
    }
    return false;
}

void move_queue_forwards(int set_number, int after_here) {
    // we do it by set number because we have to start from the start of the set
    int first_block_in_set = set_number * BLOCKS_PER_SET;
    for (int i = 0; i < BLOCKS_PER_SET; i++)
    {
        Cache[first_block_in_set + i].queue_order -= (Cache[first_block_in_set + i].queue_order > after_here);
    }
}


unsigned int get_block_address(unsigned int index, unsigned int tag){
    unsigned int address = tag;
    address << INDEX_BITS;
    address += index;
    address << OFFSET_BITS;
    return address;
}

unsigned int get_index(unsigned int address) {
    static unsigned int bitmask = 0;
    if (bitmask == 0) {
        int index_start = OFFSET_BITS;
        int index_end = index_start + INDEX_BITS - 1;
        for (int j = 0; j < INDEX_BITS; j++) {
            bitmask << 1;
            bitmask += 1;
        }
        bitmask << OFFSET_BITS;
    }
    
    address &= bitmask;
    return address >> OFFSET_BITS;

    /* ----- Or ----
     * int memory_block_number = address / BLOCK_WORDS / 4   (address / bytes_per_block)
     * return memory_block_number % CACHE_SETS
     */
}

unsigned int get_tag(unsigned int address){
    return address >> (OFFSET_BITS + INDEX_BITS);
}

void process_without_cache(){
    FILE *fp = fopen("mem_access2.txt", "r");
    char block[100];
    word t0;
    unsigned int address;
    while (fgets(block, 100, fp) != NULL){
        address = atol(&block[2]);
        if (block[0] == 'R') {
            t0 = Ram[address / BLOCK_BYTES].data[address % BLOCK_BYTES / 4]; // Ram[block][word pos]
            // if (t0 != 0)
                // printf("@@@@@@@@@@\n----------t0 not 0!---------\n---------t0: %d----------\n@@@@@@@@@@@@@@@@@@\n", t0);
        }
        else {
            t0 = atoi(strrchr(block, ' ') + 1);
            memcpy(&Ram[address / BLOCK_BYTES].data[address % BLOCK_BYTES / 4], &t0, 4);
        }
        printf("block: %s, Address: %d, t0: %d\n", block, address, t0);
    }
}


void print_config(){
    printf("Clock rate: %.2f GHz\n", CLOCK_RATE);
    printf("Access latency: %d Cycles\n", ACCESS_LATENCY);
    printf("Cache size: %d Bytes\n", CACHE_SIZE);
    printf("Line size: %d Bytes\n", BLOCK_BYTES);
    printf("Set associativity: %d\n\n", BLOCKS_PER_SET);

}

void read_config(char *file_name){
    int fd = open(file_name, O_RDONLY);
    char t[100];
    dup2(fd, 0);

    scanf("%s %f", t, &(CLOCK_RATE));
    scanf("%s %d", t, &(ACCESS_LATENCY));
    scanf("%s %d", t, &(CACHE_SIZE));
    scanf("%s %d", t, &(BLOCK_BYTES));
    scanf("%s %d", t, &(BLOCKS_PER_SET));

    CACHE_BLOCKS = CACHE_SIZE/BLOCK_BYTES;
    CACHE_SETS = CACHE_BLOCKS / BLOCKS_PER_SET;
    INDEX_BITS = (int) log2f((float)CACHE_SETS);
    OFFSET_BITS = (int) log2f((float)BLOCK_BYTES);
    BLOCK_WORDS = BLOCK_BYTES / 4;

    close(fd);
    return;
}

