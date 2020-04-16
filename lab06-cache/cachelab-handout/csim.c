#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cachelab.h"

typedef struct Block {
    unsigned long timestamp;
    unsigned long tag;
    bool valid;
    // we don't need to store data for this lab
} Block, *BlockPtr;

int s = -1;
int E = -1;
int b = -1;
bool verbose = false;

unsigned counter = 0;
unsigned hits = 0;
unsigned misses = 0;
unsigned evictions = 0;

FILE* tracefile = NULL;

BlockPtr blocks;

void help();
void parseArgs(int argc, char* argv[]);
void buildCache();
void replay();

int main(int argc, char* argv[]) {
    parseArgs(argc, argv);
    buildCache();
    replay();
    printSummary(hits, misses, evictions);
    return 0;
}

void usage() {
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
    exit(0);
}

void parseArgs(int argc, char* argv[]) {
    int opt = 0;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
            case 'h':
                usage();
                break;
            case 'v':
                verbose = true;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                if (!(tracefile = fopen(optarg, "r"))) {
                    printf("cannot open file %s\n", optarg);
                    exit(0);
                }
                break;
            default:
                usage();
        }
    }

    if (s == -1 || E == -1 || b == -1 || !tracefile) {
        printf("./csim: Missing required command line argument\n");
        usage();
    }

    printf("s: %d, E: %d, b: %d, verbose: %d\n", s, E, b, verbose);
}

void buildCache() {
    int blockCount = (1 << s) * E;
    blocks = (BlockPtr)malloc(blockCount * sizeof(Block));
    for (int i = 0; i < blockCount; i++) {
        blocks[i].tag = 0;
        blocks[i].timestamp = 0;
        blocks[i].valid = false;
    }
}

void execute(char operation, unsigned long address, unsigned size) {
    if (verbose) printf("%c %lx,%d", operation, address, size);
    unsigned long tag = address >> (s + b);
    unsigned long setBase = (address << (64 - s - b) >> (64 - s)) * E;
    // printf("tag: %lx, set: %lx\n", tag, set);
    int times = operation == 'M' ? 2 : 1;
    for (int time = 0; time < times; time++) {
        bool hit = false;
        for (int i = setBase; i < setBase + E; i++) {
            if (blocks[i].valid && blocks[i].tag == tag) {
                hit = true;
                blocks[i].timestamp = counter++;
                hits++;
                if (verbose) printf(" hit");
                break;
            }
        }
        if (!hit) {
            misses++;
            if (verbose) printf(" miss");
            // load from memory, evict if needed
            bool full = true;
            for (int i = setBase; i < setBase + E; i++) {
                if (!blocks[i].valid) {  // this block is not used yet
                    full = false;
                    blocks[i].valid = true;
                    blocks[i].tag = tag;
                    blocks[i].timestamp = counter++;
                    break;
                }
            }
            if (full) {  // evict by LRU
                evictions++;
                if (verbose) printf(" eviction");
                int min = setBase;
                for (int i = setBase + 1; i < setBase + E; i++) {
                    if (blocks[i].timestamp < blocks[min].timestamp) {
                        min = i;
                    }
                }
                blocks[min].tag = tag;
                blocks[min].timestamp = counter++;
            }
        }
    }
    if (verbose) printf("\n");
}

void replay() {
    char operation;
    unsigned long address;
    int size;
    while (!feof(tracefile)) {
        if (fscanf(tracefile, " %c %lx,%d", &operation, &address, &size) == 3) {
            if (operation == 'I') continue;
            // printf("operation:%c, address:%lx\n", operation, address);
            execute(operation, address, size);
        }
    }
    fclose(tracefile);
}
