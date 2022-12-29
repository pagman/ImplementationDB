#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "bf.h"
#include "hp_file.h"
#include "ht_table.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      fprintf(strerr, "code: %d \n", code) \
      exit(code);             \
    }                         \
  }

int my_test_hp(char * filename, int records) {
    unlink(filename);

    HP_CreateFile(filename);
    HP_info* info = HP_OpenFile(filename);
    
    if (records > 0) {
        printf("Insert Entries\n");
        
        for (int id = 0; id < records; ++id) {
            Record record = randomRecord();
            record.id = id;
            HP_InsertEntry(info, record);
        }

        printf("RUN PrintAllEntries\n");

        for (int id = 0; id < records; ++id) {
            printf("\nSearching for: %d: ", id);

            HP_GetAllEntries(info, id);       
        }
        
        int id = -1;
        
        printf("Searching for: %d (expected no match): \n", id);
        
        int b = HP_GetAllEntries(info, id); 
        
        printf("Total blocks: %d \n", b);
    }
    
    HP_CloseFile(info);
    
    return 0;
}



int my_test_ht(char * filename, int records) {
    unlink(filename);

    HT_CreateFile(filename,10);
    HT_info* info = HT_OpenFile(filename);
    
    if (records > 0) {
        printf("Insert Entries\n");
        
        for (int id = 0; id < records; ++id) {
            Record record = randomRecord();
            record.id = id;
            HT_InsertEntry(info, record);
        }

        printf("RUN PrintAllEntries\n");

        for (int id = 0; id < records; ++id) {
            printf("\nSearching for: %d: ", id);

            HT_GetAllEntries(info, id);       
        }
        
        int id = -1;
        
        printf("Searching for: %d (expected no match): \n", id);
        
        int b = HT_GetAllEntries(info, id); 
        
        printf("Total blocks: %d \n", b);
    }
    
    HT_CloseFile(info);
    
    return 0;
}

int my_test_ht_stats(char * filename) {
    HT_HashStatistics(filename);
    return 0;
}

int main() {
    BF_Init(LRU);
    
    srand(12569874);
    
    unlink("data.hp");
    unlink("data.ht");
    
    my_test_hp("data.hp", 1000);
    
    my_test_ht("data.ht", 10000);
    
    my_test_ht_stats("data.ht");
    
    BF_Close();
    
    return 0;
}
