#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

static int hp_errors = 0;

#define CALL_BF(call, printError, error_code)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    if (printError) {\
        hp_errors++; \
        BF_PrintError(code);    \
        fprintf(stderr, "code: %d \n", code); \
    }\
    return error_code;\
  } \
}

union Header {
    struct {
        char prefix[3];
        HP_info info;
    };
    char block[BF_BLOCK_SIZE];
};

typedef struct {
    int records;
    int next_block;
} HP_block_info;

static char HP_PREFIX[3] = "HP";
static int HP_ERROR = -1;

static void assignMagicWord(union Header * header) {
    strncpy(header->prefix, HP_PREFIX, strlen(HP_PREFIX) + 1);
}

static void assignDensity(union Header * header) {
    header->info.density = (BF_BLOCK_SIZE - sizeof(HP_info))/sizeof(Record);
}

static BF_Block * allocateMemoryBlock() {
    BF_Block *block = NULL;
    BF_Block_Init(&block);
    return block;
}

static int flushBlock(BF_Block **block) {
    BF_Block_SetDirty(*block);
    CALL_BF(BF_UnpinBlock(*block), true, HP_ERROR);
    BF_Block_Destroy(block);
    return BF_OK;
}

static int dumpBlock(BF_Block **block, bool unpin) {
    if (unpin) {
        CALL_BF(BF_UnpinBlock(*block), true, HP_ERROR);
    }
    BF_Block_Destroy(block);
    return BF_OK;
}

int HP_CreateFile(char *fileName) {
    const int METHOD_ERROR_CODE = HP_ERROR;
    union Header header = {0};
    BF_Block *block = allocateMemoryBlock();
    int fd1;

    assignMagicWord(&header);
    assignDensity(&header);

    CALL_BF(BF_CreateFile(fileName), true, METHOD_ERROR_CODE);
    CALL_BF(BF_OpenFile(fileName, &fd1), true, METHOD_ERROR_CODE);
    CALL_BF(BF_AllocateBlock(fd1, block), true, METHOD_ERROR_CODE);

    char * data = BF_Block_GetData(block);
    memcpy(data, &header, sizeof(union Header));
    CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);

    CALL_BF(BF_CloseFile(fd1), true, METHOD_ERROR_CODE);

    return 0;
}

HP_info* HP_OpenFile(char *fileName) {
    static HP_info * METHOD_ERROR_CODE = NULL;
    union Header * header = calloc(1, sizeof (union Header));
    BF_Block *block = allocateMemoryBlock();
    int fd1;

    CALL_BF(BF_OpenFile(fileName, &fd1), true, METHOD_ERROR_CODE);
    CALL_BF(BF_GetBlock(fd1, 0, block), true, METHOD_ERROR_CODE);
    char * data = BF_Block_GetData(block);
    memcpy((void*) header, data, sizeof(union Header));
    CALL_BF(dumpBlock(&block, true), true, METHOD_ERROR_CODE);

    header->info.fd = fd1;
    
    fprintf(stderr, "HP File opened: fd:%d, density: %d \n", header->info.fd, header->info.density); \
    
    if (strncmp(header->prefix, "HP", 2 ) != 0) {
        fprintf(stderr, "ERROR: Invalid MAGIC word :%s \n", header->prefix); \
        return NULL;
    }
    
    return (HP_info*) header;
}

int HP_CloseFile(HP_info* hp_info) {
    const int METHOD_ERROR_CODE = HP_ERROR;
    union Header * header = (union Header *) hp_info;
    int fd1 = header->info.fd;
    
    BF_Block *block = allocateMemoryBlock();
    
    CALL_BF(BF_GetBlock(fd1, 0, block), true, METHOD_ERROR_CODE);
    char * data = BF_Block_GetData(block);
    memcpy(data, (void*) header, sizeof(union Header));
    CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);
    
    CALL_BF(BF_CloseFile(fd1), true, METHOD_ERROR_CODE);
    
    free (header);
    
    printf("\nHP File closed, HP_ERRORS: %d \n", hp_errors); \
    
    return 0;
}

int HP_InsertEntry(HP_info* hp_info, Record record) {
    const int METHOD_ERROR_CODE = HP_ERROR;
    union Header * header = (union Header *) hp_info;
    int fd1 = header->info.fd;
    
    BF_Block *block = allocateMemoryBlock();
    
    const int block_num = 1 + header->info.records / header->info.density;
    const int offset = header->info.records % header->info.density;
    
    int n = BF_GetBlock(fd1, block_num, block) ;
    
    if (n != BF_OK) {
        if (n != BF_INVALID_BLOCK_NUMBER_ERROR) {
            return -1;
        }
        
        CALL_BF(BF_AllocateBlock(fd1, block), true, METHOD_ERROR_CODE);
        CALL_BF(BF_GetBlock(fd1, block_num, block), true, METHOD_ERROR_CODE);
    }
    
    char * data = BF_Block_GetData(block);
    memcpy(data + offset*sizeof(record), &record, sizeof(Record));
    
    HP_block_info * info = (HP_block_info *)(data + BF_BLOCK_SIZE - sizeof(HP_block_info));
    info->records++;
    
    CALL_BF(flushBlock(&block), true, METHOD_ERROR_CODE);
    
    printf("Inserted: ");
    printRecord(record);
    
    header->info.records++;
    
    return block_num;
}

int HP_GetAllEntries(HP_info* hp_info, int value) {
    const int METHOD_ERROR_CODE = HP_ERROR;
    union Header * header = (union Header *) hp_info;
    int fd1 = header->info.fd;
    int blocks = 0;
    bool found = false;
    
    BF_GetBlockCounter(fd1, &blocks);
    
    for (int i=1;i<blocks && !found;i++) {
        BF_Block *block = allocateMemoryBlock();
        CALL_BF(BF_GetBlock(fd1, i, block), true, METHOD_ERROR_CODE);
        char * data = BF_Block_GetData(block);
        HP_block_info * info = (HP_block_info *)(data + BF_BLOCK_SIZE - sizeof(HP_block_info));
        
        for (int j=0;j < info->records;j++) {
            Record * record = (Record *) (data + j*sizeof(Record));
            if (record->id == value) {
                printRecord(*record);
                found = true;
                break;
            }
        }
        
        CALL_BF(dumpBlock(&block, true), true, METHOD_ERROR_CODE);
    }
    
    return blocks-1;
}

