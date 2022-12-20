#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include "bf.h"
#include "ht_table.h"
#include "record.h"

static int ht_errors = 0;

#define CALL_BF(call, printError, error_code)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    if (printError) {\
        ht_errors++; \
        BF_PrintError(code);    \
        fprintf(stderr, "code: %d \n", code); \
    }\
    return error_code;\
  } \
}

union Header {

    struct {
        char prefix[3];
        HT_info info;
        int head[(BF_BLOCK_SIZE - 2 - sizeof (HT_info)) / 4];
    };
    char block[BF_BLOCK_SIZE];
};

static char HT_PREFIX[3] = "HT";
static int HT_ERROR = -1;

static void assignMagicWord(union Header * header) {
    strncpy(header->prefix, HT_PREFIX, strlen(HT_PREFIX) + 1);
}

static unsigned int hash(unsigned int x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

static void assignDensity(union Header * header) {
    header->info.density = (BF_BLOCK_SIZE - sizeof (HT_info)) / sizeof (Record);
}

static void assignBuckets(union Header * header, int buckets) {
    header->info.buckets = buckets;
}

static void assignHeads(union Header * header) {
    for (int i = 0; i < (BF_BLOCK_SIZE - 2 - sizeof (HT_info)) / 4; i++) {
        header->head[i] = -1;
    }
}

static BF_Block * allocateMemoryBlock() {
    BF_Block *block = NULL;
    BF_Block_Init(&block);
    return block;
}

static int flushBlock(BF_Block **block) {
    BF_Block_SetDirty(*block);
    CALL_BF(BF_UnpinBlock(*block), true, HT_ERROR);
    BF_Block_Destroy(block);
    return BF_OK;
}

static int dumpBlock(BF_Block **block, bool unpin) {
    if (unpin) {
        CALL_BF(BF_UnpinBlock(*block), true, HT_ERROR);
    }
    BF_Block_Destroy(block);
    return BF_OK;
}

int HT_CreateFile(char *fileName, int buckets) {
    const int METHOD_ERROR_CODE = HT_ERROR;
    union Header header = {0};
    BF_Block *block = allocateMemoryBlock();
    int fd1;
    assignMagicWord(&header);
    assignDensity(&header);
    assignBuckets(&header, buckets);
    assignHeads(&header);

    CALL_BF(BF_CreateFile(fileName), true, METHOD_ERROR_CODE);
    CALL_BF(BF_OpenFile(fileName, &fd1), true, METHOD_ERROR_CODE);
    CALL_BF(BF_AllocateBlock(fd1, block), true, METHOD_ERROR_CODE);

    char * data = BF_Block_GetData(block);
    memcpy(data, &header, sizeof (union Header));
    CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);

    CALL_BF(BF_CloseFile(fd1), true, METHOD_ERROR_CODE);

    return 0;
}

HT_info* HT_OpenFile(char *fileName) {
    static HT_info * METHOD_ERROR_CODE = NULL;
    union Header * header = calloc(1, sizeof (union Header));
    BF_Block *block = allocateMemoryBlock();
    int fd1;

    CALL_BF(BF_OpenFile(fileName, &fd1), true, METHOD_ERROR_CODE);
    CALL_BF(BF_GetBlock(fd1, 0, block), true, METHOD_ERROR_CODE);
    char * data = BF_Block_GetData(block);
    memcpy((void*) header, data, sizeof (union Header));
    CALL_BF(dumpBlock(&block, true), true, METHOD_ERROR_CODE);

    header->info.fd = fd1;

    fprintf(stderr, "HT File opened: fd:%d, density: %d \n", header->info.fd, header->info.density); \
    
    if (strncmp(header->prefix, "HT", 2) != 0) {
        fprintf(stderr, "ERROR: Invalid MAGIC word :%s \n", header->prefix); \
        return NULL;
    }

    return (HT_info*) header;
}

int HT_CloseFile(HT_info* HT_info) {
    const int METHOD_ERROR_CODE = HT_ERROR;
    union Header * header = (union Header *) HT_info;
    int fd1 = header->info.fd;

    BF_Block *block = allocateMemoryBlock();

    CALL_BF(BF_GetBlock(fd1, 0, block), true, METHOD_ERROR_CODE);
    char * data = BF_Block_GetData(block);
    memcpy(data, (void*) header, sizeof (union Header));
    CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);

    CALL_BF(BF_CloseFile(fd1), true, METHOD_ERROR_CODE);

    free(header);

    printf("\nHT File closed, HT_ERRORS: %d \n", ht_errors); \
    
    return 0;
}

int HT_InsertEntry(HT_info* ht_info, Record record) {
    const int METHOD_ERROR_CODE = HT_ERROR;
    union Header * header = (union Header *) ht_info;
    int fd1 = header->info.fd;

    int bucket = hash(record.id) % header->info.buckets;

    if (header->head[bucket] == -1) {
        int offset = 0;
        int block_num = 0;

        BF_GetBlockCounter(fd1, &block_num);

        BF_Block *block = allocateMemoryBlock();
        CALL_BF(BF_AllocateBlock(fd1, block), true, METHOD_ERROR_CODE);
        CALL_BF(BF_GetBlock(fd1, block_num, block), true, METHOD_ERROR_CODE);
        char * data = BF_Block_GetData(block);
        memcpy(data + offset * sizeof (record), &record, sizeof (Record));

        HT_block_info * info = (HT_block_info *) (data + BF_BLOCK_SIZE - sizeof (HT_block_info));
        info->records = 1;
        info->next_block = -1;
        header->head[bucket] = block_num;

        CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);

        printf("Inserted: ");
        printRecord(record);

        return 0;
    }

    int block_num = header->head[bucket];

    while (1) {
        BF_Block *block = allocateMemoryBlock();
        CALL_BF(BF_GetBlock(fd1, block_num, block), true, METHOD_ERROR_CODE);
        char * data = BF_Block_GetData(block);
        HT_block_info * info = (HT_block_info *) (data + BF_BLOCK_SIZE - sizeof (HT_block_info));

        if (info->records == header->info.density) {
            if (info->next_block != -1) {
                block_num = info->next_block;
                CALL_BF(dumpBlock(&block, true), true, METHOD_ERROR_CODE);
                continue;
            }

            int block_num = 0;

            BF_GetBlockCounter(fd1, &block_num);

            {
                int offset = 0;
                BF_Block *block = allocateMemoryBlock();
                CALL_BF(BF_AllocateBlock(fd1, block), true, METHOD_ERROR_CODE);
                CALL_BF(BF_GetBlock(fd1, block_num, block), true, METHOD_ERROR_CODE);
                char * data = BF_Block_GetData(block);
                memcpy(data + offset * sizeof (record), &record, sizeof (Record));

                HT_block_info * info = (HT_block_info *) (data + BF_BLOCK_SIZE - sizeof (HT_block_info));
                info->records = 1;
                info->next_block = -1;
                CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);
            }

            info->next_block = block_num;
            CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);
            break;
        }

        int offset = info->records;
        memcpy(data + offset * sizeof (record), &record, sizeof (Record));

        info->records++;
        CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE)
        break;
    }
    
    printf("Inserted: ");
    printRecord(record);
    return 0;
}

int HT_GetAllEntries(HT_info* ht_info, int value) {
    const int METHOD_ERROR_CODE = HT_ERROR;
    union Header * header = (union Header *) ht_info;
    int fd1 = header->info.fd;
    int blocks = 0;
    bool found = false;

    int bucket = hash(value) % header->info.buckets;

    int block_num = header->head[bucket];

    while (block_num != -1 && !found) {
        blocks++;

        BF_Block *block = allocateMemoryBlock();
        CALL_BF(BF_GetBlock(fd1, block_num, block), true, METHOD_ERROR_CODE);
        char * data = BF_Block_GetData(block);
        HT_block_info * info = (HT_block_info *) (data + BF_BLOCK_SIZE - sizeof (HT_block_info));

        for (int j = 0; j < info->records; j++) {
            Record * record = (Record *) (data + j * sizeof (Record));
            if (record->id == value) {
                printRecord(*record);
                found = true;
                break;
            }
        }

        block_num = info->next_block;

        CALL_BF(dumpBlock(&block, true), true, METHOD_ERROR_CODE);
    }

    return blocks;
}

int HT_HashStatistics(char * filename) {
    const int METHOD_ERROR_CODE = HT_ERROR;
    HT_info* ht_info = HT_OpenFile(filename);
    union Header * header = (union Header *) ht_info;
    int fd1 = header->info.fd;
    int blocks = 0;

    BF_GetBlockCounter(fd1, &blocks);

    printf("%5s %12s %12s %12s %12s %12s %12s \n", "Bucket", "Blocks", "Records", "Min/block", "Max/block", "Avg/block", "Overflow");

    int bucket_min = INT_MAX, bucket_max = 0, bucket_sum = 0, bucket_block_sum = 0, overflow_buckets = 0;

    for (int bucket = 0; bucket < header->info.buckets; bucket++) {
        int block_num = header->head[bucket];
        int min = INT_MAX, max = 0, sum = 0;
        int bucket_blocks = 0;

        while (block_num != -1) {
            bucket_blocks++;

            BF_Block *block = allocateMemoryBlock();
            CALL_BF(BF_GetBlock(fd1, block_num, block), true, METHOD_ERROR_CODE);
            char * data = BF_Block_GetData(block);
            HT_block_info * info = (HT_block_info *) (data + BF_BLOCK_SIZE - sizeof (HT_block_info));

            if (info->records < min) {
                min = info->records;
            }

            if (info->records > max) {
                max = info->records;
            }

            sum = sum + info->records;

            block_num = info->next_block;

            CALL_BF(dumpBlock(&block, true), true, METHOD_ERROR_CODE);
        }

        float avg = (float) sum / bucket_blocks;

        printf("%5d %12d %12d %12d %12d %12.2f %12s \n", bucket, bucket_blocks, sum, min, max, avg, (bucket_blocks > 1) ? "true" : "false");

        if (sum < bucket_min) {
            bucket_min = sum;
        }

        if (sum > bucket_max) {
            bucket_max = sum;
        }

        bucket_sum = bucket_sum + sum;

        bucket_block_sum = bucket_block_sum + bucket_blocks;

        if ((bucket_blocks > 1)) {
            overflow_buckets++;
        }
    }

    printf("Total file blocks          : %d \n", blocks);
    printf("Min records per bucket     : %d \n", bucket_min);
    printf("Max records per bucket     : %d \n", bucket_max);
    printf("Avg records per bucket     : %.2f \n", bucket_sum / (float) header->info.buckets);
    printf("Avg blocks  per bucket     : %.2f \n", bucket_block_sum / (float) header->info.buckets);
    printf("Total buckets with overflow: %d \n", overflow_buckets);

    HT_CloseFile(ht_info);

    return 0;
}



