#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

void read_config(char *file_name);

typedef struct settings{
    float clock_rate;
    int access_latency;
    int cache_size;
    int block_size;
    int set_associativity;
}struct_settings;

struct_settings *settings = NULL;

int main(int argc, char *argv[]) {
    settings = malloc(sizeof(struct_settings));
    printf("Hello, World!\n");

    //TODO: CHANGE THIS TO argv[1] WHEN SUBMITTING
    read_config("cachesim.conf");

    return 0;
}

void read_config(char *file_name){
    int fd = open(file_name, O_RDONLY);
    char t[1];
    dup2(fd, 0);
    scanf("%s %f", t, &settings->clock_rate);
    scanf("%s %d", t, &settings->block_size);
}
