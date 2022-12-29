#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include "bf.h"
#include "sht_table.h"
#include "ht_table.h"
#include "record.h"

static int sht_errors = 0;

#define CALL_BF(call, printError, error_code)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    if (printError) {\
        sht_errors++; \
        BF_PrintError(code);    \
        fprintf(stderr, "code: %d \n", code); \
    }\
    return error_code;\
  } \
}

typedef struct SecondaryRecord {
    char key[20];
    int block_id;
} SecondaryRecord;

union HtHeader {

    struct {
        char prefix[3];
        HT_info info;
        int head[(BF_BLOCK_SIZE - 2 - sizeof (HT_info)) / 4];
    };
    char block[BF_BLOCK_SIZE];
};

union Header {

    struct {
        char prefix[4];
        SHT_info info;
        int head[(BF_BLOCK_SIZE - 4 - sizeof (SHT_info)) / 4];
    };
    char block[BF_BLOCK_SIZE];
};

static char SHT_PREFIX[4] = "SHT";
static int SHT_ERROR = -1;

static void assignMagicWord(union Header * header) {
    strncpy(header->prefix, SHT_PREFIX, strlen(SHT_PREFIX) + 1);
}

unsigned int hash(char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

static void assignDensity(union Header * header) {
    header->info.density = (BF_BLOCK_SIZE - sizeof (HT_info)) / sizeof (Record);
}

static void assignBuckets(union Header * header, int buckets) {
    header->info.buckets = buckets;
}

static void assignAttribute(union Header * header, char * record_attribute) {
    strcpy(header->info.record_attribute, record_attribute);
}

static void assignDatafile(union Header * header, char * fileName) {
    strcpy(header->info.primary_data_file, fileName);
}

static void assignHeads(union Header * header) {
    for (int i = 0; i < (BF_BLOCK_SIZE - 4 - sizeof (SHT_info)) / 4; i++) {
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
    CALL_BF(BF_UnpinBlock(*block), true, SHT_ERROR);
    BF_Block_Destroy(block);
    return BF_OK;
}

static int dumpBlock(BF_Block **block, bool unpin) {
    if (unpin) {
        CALL_BF(BF_UnpinBlock(*block), true, SHT_ERROR);
    }
    BF_Block_Destroy(block);
    return BF_OK;
}

int SHT_CreateSecondaryIndex(char *sfileName, char * record_attribute, int buckets, char* fileName) {
    if (strlen(record_attribute) >= 15) {
        return SHT_ERROR;
    }

    const int METHOD_ERROR_CODE = SHT_ERROR;
    union Header header = {0};
    BF_Block *block = allocateMemoryBlock();
    int fd1;
    assignMagicWord(&header);
    assignDensity(&header);
    assignBuckets(&header, buckets);
    assignAttribute(&header, record_attribute);
    assignDatafile(&header, fileName);
    assignHeads(&header);

    CALL_BF(BF_CreateFile(sfileName), true, METHOD_ERROR_CODE);
    CALL_BF(BF_OpenFile(sfileName, &fd1), true, METHOD_ERROR_CODE);
    CALL_BF(BF_AllocateBlock(fd1, block), true, METHOD_ERROR_CODE);

    char * data = BF_Block_GetData(block);
    memcpy(data, &header, sizeof (union Header));
    CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);

    CALL_BF(BF_CloseFile(fd1), true, METHOD_ERROR_CODE);

    return 0;

}

SHT_info* SHT_OpenSecondaryIndex(char *fileName) {
    static SHT_info * METHOD_ERROR_CODE = NULL;
    union Header * header = calloc(1, sizeof (union Header));
    BF_Block *block = allocateMemoryBlock();
    int fd1;

    CALL_BF(BF_OpenFile(fileName, &fd1), true, METHOD_ERROR_CODE);
    CALL_BF(BF_GetBlock(fd1, 0, block), true, METHOD_ERROR_CODE);
    char * data = BF_Block_GetData(block);
    memcpy((void*) header, data, sizeof (union Header));
    CALL_BF(dumpBlock(&block, true), true, METHOD_ERROR_CODE);

    header->info.fd = fd1;

    fprintf(stderr, "SHT File opened, primary index:%s, foreign key:%s : fd:%d, density: %d \n", header->info.primary_data_file, header->info.record_attribute, header->info.fd, header->info.density); \
    
    if (strncmp(header->prefix, "SHT", 3) != 0) {
        fprintf(stderr, "ERROR: Invalid MAGIC word :%s \n", header->prefix); \
        return NULL;
    }

    return (SHT_info*) header;
}

int SHT_CloseSecondaryIndex(SHT_info* SHT_info) {
    const int METHOD_ERROR_CODE = SHT_ERROR;
    union Header * header = (union Header *) SHT_info;
    int fd1 = header->info.fd;

    BF_Block *block = allocateMemoryBlock();

    CALL_BF(BF_GetBlock(fd1, 0, block), true, METHOD_ERROR_CODE);
    char * data = BF_Block_GetData(block);
    memcpy(data, (void*) header, sizeof (union Header));
    CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);

    CALL_BF(BF_CloseFile(fd1), true, METHOD_ERROR_CODE);

    free(header);

    printf("\nSHT File closed, SHT_ERRORS: %d \n", sht_errors); \
    
    return 0;
}

int SHT_SecondaryInsertEntry(SHT_info* sht_info, Record original_record, int block_id) {
    const int METHOD_ERROR_CODE = SHT_ERROR;
    union Header * header = (union Header *) sht_info;
    int fd1 = header->info.fd;

    SecondaryRecord record = {0};
    record.block_id = block_id;

    if (strcmp(header->info.record_attribute, "record") == 0) {
        strcpy(record.key, original_record.record);
    } else if (strcmp(header->info.record_attribute, "name") == 0) {
        strcpy(record.key, original_record.name);
    } else if (strcmp(header->info.record_attribute, "surname") == 0) {
        strcpy(record.key, original_record.surname);
    } else if (strcmp(header->info.record_attribute, "city") == 0) {
        strcpy(record.key, original_record.city);
    } else {
        return METHOD_ERROR_CODE;
    }

    int bucket = hash(record.key) % header->info.buckets;

    if (header->head[bucket] == -1) {
        int offset = 0;
        int block_num = 0;

        BF_GetBlockCounter(fd1, &block_num);

        BF_Block *block = allocateMemoryBlock();
        CALL_BF(BF_AllocateBlock(fd1, block), true, METHOD_ERROR_CODE);
        CALL_BF(BF_GetBlock(fd1, block_num, block), true, METHOD_ERROR_CODE);
        char * data = BF_Block_GetData(block);
        memcpy(data + offset * sizeof (record), &record, sizeof (SecondaryRecord));

        SHT_block_info * info = (SHT_block_info *) (data + BF_BLOCK_SIZE - sizeof (SHT_block_info));
        info->records = 1;
        info->next_block = -1;
        header->head[bucket] = block_num;

        CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);

        printf("Inserted (secondary index): (%s,%d) \n", record.key, record.block_id);

        header->info.records++;
        
        return 0;
    }

    int block_num = header->head[bucket];

    while (1) {
        BF_Block *block = allocateMemoryBlock();
        CALL_BF(BF_GetBlock(fd1, block_num, block), true, METHOD_ERROR_CODE);
        char * data = BF_Block_GetData(block);
        SHT_block_info * info = (SHT_block_info *) (data + BF_BLOCK_SIZE - sizeof (SHT_block_info));

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
                memcpy(data + offset * sizeof (record), &record, sizeof (SecondaryRecord));

                SHT_block_info * info = (SHT_block_info *) (data + BF_BLOCK_SIZE - sizeof (SHT_block_info));
                info->records = 1;
                info->next_block = -1;
                CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);
            }

            info->next_block = block_num;
            CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);
            break;
        }

        int offset = info->records;
        memcpy(data + offset * sizeof (record), &record, sizeof (SecondaryRecord));

        info->records++;
        CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE)
        break;
    }

    printf("Inserted (secondary index): (%s,%d) \n", record.key, record.block_id);
    
    header->info.records++;
    
    return 0;
}

int SHT_SecondaryGetAllEntries(HT_info* ht_info, SHT_info* sht_info, char* value) {
    const int METHOD_ERROR_CODE = SHT_ERROR;
    union Header * header = (union Header *) sht_info;
    union HtHeader * htheader = (union HtHeader *) ht_info;
    int fd1 = header->info.fd;
    int fd2 = htheader->info.fd;
    int rows2 = htheader->info.records;
    int blocks = 0;

    int bucket = hash(value) % header->info.buckets;

    int block_num = header->head[bucket];

    int * cache = (int *) malloc(sizeof (int)*(rows2+1));

    for (int i = 0; i < rows2 + 1; i++) {
        cache[i] = 0;
    }

    while (block_num != -1) {
        blocks++;

        BF_Block *block = allocateMemoryBlock();
        CALL_BF(BF_GetBlock(fd1, block_num, block), true, METHOD_ERROR_CODE);
        char * data = BF_Block_GetData(block);
        SHT_block_info * info = (SHT_block_info *) (data + BF_BLOCK_SIZE - sizeof (SHT_block_info));

        for (int j = 0; j < info->records; j++) {
            SecondaryRecord * record = (SecondaryRecord *) (data + j * sizeof (SecondaryRecord));

            bool matches = false;

            if (strcmp(header->info.record_attribute, "record") == 0) {
                if (strcmp(record->key, value) == 0) {
                    matches = true;
                }
            } else if (strcmp(header->info.record_attribute, "name") == 0) {
                if (strcmp(record->key, value) == 0) {
                    matches = true;
                }
            } else if (strcmp(header->info.record_attribute, "surname") == 0) {
                if (strcmp(record->key, value) == 0) {
                    matches = true;
                }
            } else if (strcmp(header->info.record_attribute, "city") == 0) {
                if (strcmp(record->key, value) == 0) {
                    matches = true;
                }
            } else {
                continue;
            }

            if (matches) {
                BF_Block *block = allocateMemoryBlock();
                int blocknum = record->block_id;
                printf("Match: (%s,%d) : ", record->key, record->block_id);

                CALL_BF(BF_GetBlock(fd2, blocknum, block), true, METHOD_ERROR_CODE);
                char * data = BF_Block_GetData(block);
                HT_block_info * info = (HT_block_info *) (data + BF_BLOCK_SIZE - sizeof (HT_block_info));

                bool matches = false;
                
                for (int j = 0; j < info->records; j++) {
                    Record * record = (Record *) (data + j * sizeof (Record));

                    if (strcmp(header->info.record_attribute, "record") == 0) {
                        if (strcmp(record->record, value) == 0) {
                            matches = true;
                        }
                    } else if (strcmp(header->info.record_attribute, "name") == 0) {
                        if (strcmp(record->name, value) == 0) {
                            matches = true;
                        }
                    } else if (strcmp(header->info.record_attribute, "surname") == 0) {
                        if (strcmp(record->surname, value) == 0) {
                            matches = true;
                        }
                    } else if (strcmp(header->info.record_attribute, "city") == 0) {
                        if (strcmp(record->city, value) == 0) {
                            matches = true;
                        }
                    }
                    
                    if (matches && cache[record->id] == 0) {
                        cache[record->id] = 1;
                        printRecord(*record);
                        break;
                    }                    
                }
                
                CALL_BF(dumpBlock(&block, true), true, METHOD_ERROR_CODE);
            }
        }

        block_num = info->next_block;

        CALL_BF(dumpBlock(&block, true), true, METHOD_ERROR_CODE);
    }

    free(cache);
    return blocks;
}


int SHT_HashStatistics(char * filename) {
    const int METHOD_ERROR_CODE = SHT_ERROR;
    SHT_info* sht_info = SHT_OpenSecondaryIndex(filename);
    union Header * header = (union Header *) sht_info;
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
            SHT_block_info * info = (SHT_block_info *) (data + BF_BLOCK_SIZE - sizeof (SHT_block_info));

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

    SHT_CloseSecondaryIndex(sht_info);

    return 0;
}






