#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define MEMORY_SIZE 64*1024*1024    // addresses are 26 bits (4 bytes)
typedef unsigned int word;

void read_word(unsigned int address);
void write_word(int address, word word_to_write);
void read_config(char *file_name);
void print_config();
void process_without_cache();
void allocate_memory();
void allocate_cache();
void process_with_cache();
int evict_LRU(unsigned int set_number, int ram_block_number);
int find_free_space(unsigned int set_number, unsigned int tag);
int is_hit(int set_number, int tag);
void move_queue_forwards(int set_number, int after_here);
unsigned int get_index(unsigned int address);
unsigned int get_tag(unsigned int address);
unsigned int get_block_address(unsigned int index, unsigned int tag);
void read_into_cache_block(int cache_block, int ram_block_number);
void write_word_to_cache(int address, int block_number, word word);
void move_to_back_of_queue(int set_number, int block_number);
int count_bits(int x);
void print_results();

int ram_access = 0;
int hits = 0;
int misses = 0;

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

typedef struct block{
    bool occupied;      // == valid
    int tag;            // at most 24 bits, fits in a 32-bit int
    int queue_order;    // at most 26 bits, fits in a 32-bit int
    word *data;          // block size is 1, 2, 4, .. words (ints)
}block;

block *Ram;
block *Cache;

int main(int argc, char *argv[]) {
    //TODO: CHANGE THIS TO argv[1] WHEN SUBMITTING
    read_config("C:\\Users\\jjh-L\\CLionProjects\\ComputerArchitecture-Cache\\cachesim.conf");
    print_config();

    allocate_memory();
    process_without_cache();

    allocate_cache();
    process_with_cache();

    print_results();


    return 0;
}

void print_results(){
    // Without cache, 1 memory access per read, 1 memory access per write
    printf("Average memory access latency without cache: %.1f ns\n", (float) ACCESS_LATENCY / CLOCK_RATE);

    printf("* L1 Cache Contents\n");
    for (int i = 0; i < CACHE_BLOCKS; i++) {
        printf("%d: ", i);
        for (int j = 0; j < BLOCK_WORDS; j++) {
            printf("%08x ", Cache[i].data[j]);
        }
        printf("\n");
    }

    // TODO printf("\n\nTotal program run time: %.1f seconds");
    printf("L1 total access: %d\n", hits + misses);
    printf("L1 total miss count: %d\n", misses);
    printf("L1 miss rate: %.2f\n", (float) misses / (misses + hits));
                                                            // ---------Total cycles--------------            /  instructions   /  cycles per ns
    printf("Average memory access latency: %.2f ns\n", (ram_access * ACCESS_LATENCY +  2.0 * (misses + hits)) / (misses + hits) / CLOCK_RATE);

}


void process_with_cache(){
    FILE *fp = fopen("C:\\Users\\jjh-L\\CLionProjects\\ComputerArchitecture-Cache\\mem_access3.txt", "r");
    char char_buf[100];
    int address;

    while (fgets(char_buf, 100, fp) != NULL){
        address = atoi(&char_buf[2]);

        if (char_buf[0] == 'R') {
            read_word(address);
        }
        else {
            write_word(address, atoi(strrchr(char_buf, ' ') + 1));
        }
    }
    printf("Hits: %d, Misses: %d\n", hits, misses);
}

void read_word(unsigned int address) {
    int ram_block_number = address / BLOCK_BYTES;
    int index = get_index(address);   // = set number
    int tag = get_tag(address);
    int evicted_block, free_block;

    // Now we have to look inside the set whether it's a hit
    int hit = is_hit(index, tag);

    // If its a hit, theres no need to access the memory.
    if (hit) {
        // We move the hit block to the back of the queue in the hit function.
        move_to_back_of_queue(index, hit);
        return;
    }
    // If its a miss
    if (!hit){
        // If there's a free space in the set
        if ((free_block = find_free_space(index, tag))) {
            read_into_cache_block(free_block, ram_block_number);
        }
        else {
            // If there isn't
            evicted_block = evict_LRU(index, ram_block_number);
            move_to_back_of_queue(index, evicted_block);
            read_into_cache_block(evicted_block, ram_block_number);
            Cache[evicted_block].tag = tag;
        }
    }
}

void write_word(int address, word word_to_write){
    int ram_block_number = address / BLOCK_BYTES;
    int index = get_index(address);   // = set number
    int tag = get_tag(address);
    int evicted_block, free_block;

    // Now we have to look inside the set whether it's a hit
    int hit = is_hit(index, tag);

    if (hit) {
        write_word_to_cache(address, hit, word_to_write);
        move_to_back_of_queue(index, hit);
        return;
    }

    if (!hit) {
        // if there's a free space
        if ((free_block = find_free_space(index, tag))) {
            if (BLOCK_WORDS > 1) {
                read_into_cache_block(free_block, ram_block_number);
            }
            write_word_to_cache(address, free_block, word_to_write);
        }
        else {
            evicted_block = evict_LRU(index, ram_block_number);
            move_to_back_of_queue(index, evicted_block);
            if (BLOCK_WORDS > 1) {
                read_into_cache_block(evicted_block, ram_block_number);
            }
            write_word_to_cache(address, evicted_block, word_to_write);
        }
    }
}

void write_word_to_cache(int address, int block_number, word word){
    memcpy(&Cache[block_number].data[(address % BLOCK_BYTES) / 4], &word, 4);
}

void move_to_back_of_queue(int set_number, int block_number) {
    move_queue_forwards(set_number, Cache[block_number].queue_order);
    Cache[block_number].queue_order = BLOCKS_PER_SET;
}


// Search for the first block in the queue, then write it into the memory
int evict_LRU(unsigned int set_number, int ram_block_number){
    int first_block_in_set = set_number * BLOCKS_PER_SET;
    int block_to_evict = 0;

    for (int i = 0; i < BLOCKS_PER_SET; i++) {
        if (Cache[first_block_in_set + i].queue_order == 1) {
            block_to_evict = first_block_in_set + i;
            break;
        }
    }

    memcpy(Ram[ram_block_number].data, Cache[block_to_evict].data, BLOCK_BYTES);
    ram_access += 1;
    return block_to_evict;
}

int find_free_space(unsigned int set_number, unsigned int tag) {
    int first_block_in_set = set_number * BLOCKS_PER_SET;
    for (int i = 0; i < BLOCKS_PER_SET; ++i)
    {
        if (!Cache[first_block_in_set + i].occupied) {
            Cache[first_block_in_set + i].occupied = true;
            Cache[first_block_in_set + i].tag = tag;
            Cache[first_block_in_set + i].queue_order = i + 1;
            return first_block_in_set + i;
        }
    }
    return 0;
}

// Place the data from the memory into an unoccupied slot
void read_into_cache_block(int cache_block, int ram_block_number) {
    memcpy(Cache[cache_block].data, Ram[ram_block_number].data, BLOCK_BYTES);
    ram_access += 1;
}


// Returns: Hit block number if hit, 0 if miss
int is_hit(int set_number, int tag) {
    int first_block_in_set = set_number * BLOCKS_PER_SET;
    for (int i = 0; i < BLOCKS_PER_SET; ++i) {
        if (Cache[first_block_in_set + i].occupied && Cache[first_block_in_set + i].tag == tag)
        {
            hits++;
            return first_block_in_set + i;
        }
    }
    misses++;
    return 0;
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
    address <<= INDEX_BITS;
    address += index;
    address <<= OFFSET_BITS;
    return address;
}

unsigned int get_index(unsigned int address) {
    static unsigned int bitmask = 0;
    if (bitmask == 0) {
        for (int j = 0; j < INDEX_BITS; j++) {
            bitmask <<= 1;
            bitmask += 1;
        }
        bitmask <<= OFFSET_BITS;
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

void allocate_memory(){
    Ram = calloc(sizeof(struct block), MEMORY_SIZE / BLOCK_BYTES);

    for (long int i = 0; i < MEMORY_SIZE / BLOCK_BYTES; ++i)
    {
        Ram[i].data = calloc(sizeof(word), BLOCK_WORDS);
    }
}


void allocate_cache(){
    Cache = calloc(sizeof(struct block), CACHE_BLOCKS);

    for (long int i = 0; i < CACHE_BLOCKS; ++i)
    {
        Cache[i].data = calloc(sizeof(word), BLOCK_WORDS);
        Cache[i].queue_order = 1;
    }
}

void process_without_cache(){
    FILE *fp = fopen("C:\\Users\\jjh-L\\CLionProjects\\ComputerArchitecture-Cache\\mem_access3.txt", "r");
    char char_buf[100];
    word t0;
    int address;

    while (fgets(char_buf, 100, fp) != NULL){
        address = atoi(&char_buf[2]);
        if (char_buf[0] == 'R') {
            t0 = Ram[address / BLOCK_BYTES].data[(address % BLOCK_BYTES) / 4]; // Ram[block][word pos]
            // if (t0 != 0)
                // printf("@@@@@@@@@@\n----------t0 not 0!---------\n---------t0: %d----------\n@@@@@@@@@@@@@@@@@@\n", t0);
        }

            // TODO -> We should only write the word, not the whole block
        else {
            t0 = atoi(strrchr(char_buf, ' ') + 1);
            memcpy(&Ram[address / BLOCK_BYTES].data[address % BLOCK_BYTES / 4], &t0, 4);
        }
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
    /*int fd = open(file_name, O_RDONLY);
    char t[100];
    dup2(fd, 0);

    scanf("%s %f", t, &(CLOCK_RATE));
    scanf("%s %d", t, &(ACCESS_LATENCY));
    scanf("%s %d", t, &(CACHE_SIZE));
    scanf("%s %d", t, &(BLOCK_BYTES));
    scanf("%s %d", t, &(BLOCKS_PER_SET));*/

    FILE *fp = fopen(file_name, "r");
    char t[100];
    fscanf(fp, "%s %f", t, &(CLOCK_RATE));
    fscanf(fp, "%s %d", t, &(ACCESS_LATENCY));
    fscanf(fp, "%s %d", t, &(CACHE_SIZE));
    fscanf(fp, "%s %d", t, &(BLOCK_BYTES));
    fscanf(fp, "%s %d", t, &(BLOCKS_PER_SET));


    CACHE_BLOCKS = CACHE_SIZE/BLOCK_BYTES;
    CACHE_SETS = CACHE_BLOCKS / BLOCKS_PER_SET;
    //INDEX_BITS = (int) log2(CACHE_SETS) + 1;
    INDEX_BITS = count_bits(CACHE_SETS - 1);
    OFFSET_BITS = count_bits(BLOCK_BYTES - 1);
    BLOCK_WORDS = BLOCK_BYTES / 4;

    fclose(fp);
}

int count_bits(int x){
    int bits = 0;
    while (x) {
        bits++;
        x >>= 1;
    }
    return bits;
}

