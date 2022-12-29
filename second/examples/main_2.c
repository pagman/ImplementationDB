#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "bf.h"
#include "ht_table.h"
#include "sht_table.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

int my_test_sht(char * filename, char * index_filename, int records) {
    HT_CreateFile(filename, 10);
    SHT_CreateSecondaryIndex(index_filename, "surname", 10, filename);

    
    HT_info* info = HT_OpenFile(filename);
    SHT_info* index_info = SHT_OpenSecondaryIndex(index_filename);
    
    if (records > 0) {
        printf("Insert Entries\n");
        
        Record * recordTable = malloc(sizeof(Record)*records);

        for (int id = 0; id < records; ++id) {
            Record record = randomRecord();
            record.id = id;
            int block_id = HT_InsertEntry(info, record);
            SHT_SecondaryInsertEntry(index_info, record, block_id);
            
            recordTable[id] = record;
        }

        printf("RUN PrintAllEntries (primary key) \n");

        for (int id = 0; id < records; ++id) {
            printf("Searching for %5d, result:  ", id);

            HT_GetAllEntries(info, id);
        }

        int id = -1;

        printf("Searching for: %d (expected no match): \n", id);

        int b = HT_GetAllEntries(info, id);

        printf("Total blocks: %d \n", b);

        printf("RUN SHT_SecondaryGetAllEntries (secondary key) \n");
        
        for (int id = 0; id < records; ++id) {
            Record record = recordTable[id];
            printf("Searching for name %-20s - result:  \n", record.surname);
            SHT_SecondaryGetAllEntries(info, index_info, record.surname);
        }

        free(recordTable);
    }

    SHT_CloseSecondaryIndex(index_info);
    HT_CloseFile(info);

    return 0;
}

static int my_test_ht_stats(char * filename) {
    HT_HashStatistics(filename);
    return 0;
}

static int my_test_sht_stats(char * filename) {
    SHT_HashStatistics(filename);
    return 0;
}

int main() {
    BF_Init(LRU);

    srand(12569874);

    unlink("data.ht");
    unlink("index.db");

    my_test_sht("data.ht", "index.db", 1000);

    my_test_ht_stats("data.ht");
    
    my_test_sht_stats("index.db");
    
    BF_Close();

    return 0;
}
