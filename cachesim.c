// 교수한테 할 질문
// 1. Clock rate는 정수인지
// 2. Memory 64MB 할당할 때 64*1024^2 해야하는지 아니면 그냥 64*1000*1000
// 3. "Line size (block size)는 2의 지수승개이다"라고 쓰여 있는데 사실 4의 지수승개죠?

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_SIZE 64000000

void read_config(char *file_name);
void print_config();
void process_without_cache();

typedef struct settings{
    float clock_rate;
    int access_latency;
    int cache_size;
    int block_size;
    int set_associativity;
}struct_settings;

struct_settings *settings = NULL;
char **start_of_memory;

int main(int argc, char *argv[]) {
    settings = malloc(sizeof(struct_settings));

    //TODO: CHANGE THIS TO argv[1] WHEN SUBMITTING
    read_config("cachesim.conf");
    print_config();

    start_of_memory = calloc(sizeof(char *), MEMORY_SIZE / settings->block_size);

    for (long int i = 0; i < MEMORY_SIZE / settings->block_size; ++i)
    {
        start_of_memory[i] = calloc(settings->block_size, 1);
    }

    process_without_cache();

    return 0;
}

void print_config(){
    printf("Clock rate: %.2f GHz\n", settings->clock_rate);
    printf("Access latency: %d Cycles\n", settings->access_latency);
    printf("Cache size: %d Bytes\n", settings->cache_size);
    printf("Block size: %d Bytes\n", settings->block_size);
    printf("Set associativity: %d\n\n", settings->set_associativity);

}

void read_config(char *file_name){
    int fd = open(file_name, O_RDONLY);
    char t[100];
    dup2(fd, 0);

    scanf("%s %f", t, &(settings->clock_rate));
    scanf("%s %d", t, &(settings->access_latency));
    scanf("%s %d", t, &(settings->cache_size));
    scanf("%s %d", t, &(settings->block_size));
    scanf("%s %d", t, &(settings->set_associativity));

    close(fd);
    return;
}

void process_without_cache(){
    FILE *fp = fopen("mem_access2.txt", "r");
    char line[100];
    int t0;
    int address;
    while (fgets(line, 100, fp) != NULL){
        address = atol(&line[2]);
        if (line[0] == 'R') {
            t0 = start_of_memory[address/settings->block_size][address % settings->block_size];
            if (t0 != 0)
                printf("@@@@@@@@@@\n----------t0 not 0!---------\n---------t0: %d----------\n@@@@@@@@@@@@@@@@@@\n", t0);
        }
        else {
            t0 = atoi(strrchr(line, ' ') + 1);
            memcpy(&start_of_memory[address/settings->block_size][address % settings->block_size], &t0, 4);
        }
        printf("Line: %s, Address: %d, t0: %d\n", line, address, t0);
    }
}