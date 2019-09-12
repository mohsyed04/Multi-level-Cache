#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#define MIN_ARGS_REQUIRED   10

#define YES 1
#define NO 0

#define single 0
#define multi_level 1

#ifdef DEBUG
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

typedef struct Block //single block
{
    
    uint64_t tag;
    uint8_t valid_bit;
    char dirty_bit;
    uint64_t age;
    int set_number;
}Block;


int m;
int no_of_blocks_L1;
int no_of_blocks_L2;
uint16_t number_of_sets_L1;
uint16_t number_of_sets_L2;
uint16_t BLOCK_SIZE;
int L1_SIZE;
int L1_ASSOC;
int L2_SIZE;
int L2_ASSOC;
int Replacement_Policy;
int Inclusion_Property;

int memory_traffic=0;
int L1_write_backs=0;
int L1_reads=0;
int L1_read_misses=0;
int L1_writes=0;
int L1_write_misses=0;

int L2_write_backs=0;
int L2_reads=0;
int L2_read_misses=0;
int L2_writes=0;
int L2_write_misses=0;

struct Block *L1 = NULL;
struct Block *L2 = NULL; /* sizeof(Block) */

const char *filePath = NULL;
FILE *fp = 0;

uint16_t log_base_2(uint16_t n)
{
    uint16_t logValue = -1;
    if (n == 0)
    {
        return(-1);
    }
    while(n){
        logValue++;
        n>>=1;
    }
    return logValue;
}

void openfile()
{
    fp = fopen(filePath, "r");
    if (fp) {
        DEBUG_PRINT(("File opened: %s\n", filePath));
    }
}

void number_of_blocks() {
    if (L1_SIZE > 0)
    {
        no_of_blocks_L1 = L1_SIZE/BLOCK_SIZE;
        
    }
    
    if (L2_SIZE > 0)
    {
        no_of_blocks_L2 = L2_SIZE/BLOCK_SIZE;
    }
}

void number_of_sets() {
    
    number_of_sets_L1 = no_of_blocks_L1/L1_ASSOC;
    if (L2_SIZE > 0)
    {
        number_of_sets_L2 = no_of_blocks_L2/L2_ASSOC;
    }
    
}

void initialize_blocks_L1() {
    for(int i =0; i<no_of_blocks_L1; i++) {
        L1[i].valid_bit = 0;
        L1[i].age = 0;
        /*L1[i].dirty_bit = 0;*/
    }
}

void increment_age_blocks_L1() {
    for(int i =0; i<no_of_blocks_L1; i++) {
        L1[i].age++;
        /*L1[i].dirty_bit = 0;*/
    }
}
void increment_age_blocks_L2() {
    for(int i =0; i<no_of_blocks_L2; i++) {
        L1[i].age++;
        /*L1[i].dirty_bit = 0;*/
    }
}


void initialize_blocks_L2() {
    for(int i =0; i<no_of_blocks_L2; i++) {
        L2[i].valid_bit = 0;
        L2[i].age = 0;
        /*L2[i].dirty_bit = 0;*/
    }
}

void initialize_set_numbers_L1() {
    int set_no = 0;
    int count =0;
    for (int i = 0; i < no_of_blocks_L1; i++)
    {
        if (count == L1_ASSOC)
        {
            count=0;
            set_no++;
        }
        L1[i].set_number = set_no;
        L1[i].age = 0;
        count++;
    }
}
void initialize_set_numbers_L2() {
    int set_no = 0;
    int count =0;
    for (int i = 0; i < no_of_blocks_L2; i++)
    {
        if (count == L2_ASSOC)
        {
            count=0;
            set_no++;
        }
        L2[i].set_number = set_no;
        L2[i].age = 0;
        count++;
    }
}
void read_L2(uint64_t addr, int x, int y, int z)
{
    L2_reads++;
    if(L2_SIZE == 0) {
		memory_traffic++;    
		return;
    }
    unsigned mask_set_index = (1 << y) -1 << x; //mask to get the middle y bits
    unsigned mask_tag_bits = ((1<<z) -1); 
    //uint32_t block_offset = addr & mask_last_bits ;
    uint16_t idx = (addr & mask_set_index) >>x;
    uint32_t tag_bits = (addr >> (x+y)) & mask_tag_bits;
    
    uint16_t index = idx << log_base_2(L2_ASSOC); // to get the block starting address
    increment_age_blocks_L2();
    
    for (int i = index; i < (index + L1_ASSOC); i++)
    {
        if(L2[i].valid_bit == 1 && L2[i].tag == tag_bits  ) //if hit
        {
            L2[i].age = 0; //set age to max counter value
            return;
        }
        
        if (L2[i].valid_bit == 0 && (index/L2_ASSOC) == L2[i].set_number) //if miss and an empty block is available
        {
            memory_traffic++;//read_L2(addr,x,y,z); //issue a read to the lower memory
            L2[i].tag = tag_bits; //allocate missed block in the empty frame
            L2[i].valid_bit=1;
            L2[i].age = 0;
            L2_read_misses++;
            return;
        }
    }
    
    uint64_t max_age = 0;
    int max_age_index = 0;
    for (int i = index; i < (index + L2_ASSOC); i++)
    {
        if (L2[i].age > max_age) {
            max_age = L2[i].age;
            max_age_index = i;
        }
    }

    if (L2[max_age_index].dirty_bit == 'D')
    {
        memory_traffic++; //write_L2(L1[max_age_index].tag,x,y,z);
    }
    memory_traffic++;  //read_L2(addr,x,y,z);
    L2[max_age_index].tag = tag_bits;
    L2[max_age_index].valid_bit = 1;
    L2_read_misses++;
    L2[max_age_index].age = 0;
    return;
}

void write_L2(uint64_t addr, int x, int y, int z)
{
    L2_writes++;
    if(L2_SIZE == 0) {
		memory_traffic++; 
		return;   
    }
    unsigned mask_set_index = ((1<<y) -1) << x; // mask to get the middle y bits starting at the end of tag bits
    unsigned mask_tag_bits = ((1 << z) -1);
    uint16_t idx = (addr & mask_set_index) >> x;
    uint32_t tag_bits = (addr >> (x+y)) & mask_tag_bits;

    uint16_t index = idx << log_base_2(L2_ASSOC); // to get the block starting address
    increment_age_blocks_L2();

    for (int i = index; i < (index + L2_ASSOC); i++)
    {
        if (L2[i].valid_bit == 1 && L2[i].tag == tag_bits ) //if hit
        {
            L2[i].dirty_bit = 'D';
            L2[i].age = 0; //set age to max counter value
            return;
        }
        if (L2[i].valid_bit == 0 && (index/L2_ASSOC) == L2[i].set_number) //if miss and an empty block is available
        {
            memory_traffic++; //read_L2(addr,x,y,z);
            L2[i].tag = tag_bits; //allocate missed block in the empty frame
            L2[i].dirty_bit = 'D';
            L2[i].valid_bit=1;
            L2[i].age = 0;
            L2_write_misses++;
            return;
        }
        
    }
    
    uint64_t max_age = 0;
    int max_age_index = 0;
    for (int i = index; i < (index + L2_ASSOC); i++)
    {
        if (L2[i].age > max_age) {
            max_age = L2[i].age;
            max_age_index = i;
        }
    }

    if (L2[max_age_index].dirty_bit == 'D')
    {
        memory_traffic++;//write_L2(L2[max_age_index].tag,x,y,z);
    }
    memory_traffic++;//read_L2(addr,x,y,z);
    L2[max_age_index].tag = tag_bits;
    L2[max_age_index].dirty_bit = 'D';
    L2[max_age_index].valid_bit = 1;
    L2_write_misses++;
    L2[max_age_index].age = 0; //set age to max counter value
    return;
}


//BLOCK_SIZE = 16;
//L1_SIZE = 1024;
//L1_ASSOC = 2;
//L2_SIZE = 0;
//L2_ASSOC = 0;
//read_L1(address_int,no_of_offset_bits, no_of_index_bits_L1, no_of_tag_bits_L1, no_of_index_bits_L2, no_of_tag_bits_L2);
void read_L1(uint64_t addr, int x, int y, int z,int y1, int z1)
{
    L1_reads++;
    unsigned mask_set_index = ((1<<y) -1) << x; // mask to get the middle y bits starting at the end of tag bits
    unsigned mask_tag_bits = ((1 << z) -1);
    uint16_t idx = (addr & mask_set_index) >> x;
    uint32_t tag_bits = (addr >> (x+y)) & mask_tag_bits;

    uint16_t index = idx << log_base_2(L1_ASSOC); // to get the block starting address
    increment_age_blocks_L1();

    for (int i = index; i < (index + L1_ASSOC); i++)
    {
        if(L1[i].valid_bit == 1 && L1[i].tag == tag_bits  ) //if hit
        {
            L1[i].age = 0; //set age to max counter value
            return;
        }
        
        if (L1[i].valid_bit == 0 && (index/L1_ASSOC) == L1[i].set_number) //if miss and an empty block is available
        {
            read_L2(addr,x,y1,z1);
            L1[i].tag = tag_bits; //allocate missed block in the empty frame
            L1[i].valid_bit=1;
            L1[i].age = 0;
            L1_read_misses++;
            return;
        }
    }
    
    uint64_t max_age = 0;
    int max_age_index = 0;
    for (int i = index; i < (index + L1_ASSOC); i++)
    {
        if (L1[i].age > max_age) {
            max_age = L1[i].age;
            max_age_index = i;
        }
    }

    if (L1[max_age_index].dirty_bit == 'D')
    {
        write_L2(L1[max_age_index].tag,x,y,z);
        L1_write_backs++;
    }
    read_L2(addr,x,y1,z1);
    L1[max_age_index].tag = tag_bits;
    L1[max_age_index].valid_bit = 1;
    L1_read_misses++;
    L1[max_age_index].age = 0;
    return;
}

void write_L1(uint64_t addr, int x, int y, int z,int y1, int z1)
{
    L1_writes++;
    unsigned mask_set_index = ((1<<y) -1) << x; // mask to get the middle y bits starting at the end of tag bits
    unsigned mask_tag_bits = ((1 << z) -1);
    uint16_t idx = (addr & mask_set_index) >> x;
    uint32_t tag_bits = (addr >> (x+y)) & mask_tag_bits;

    uint16_t index = idx << log_base_2(L1_ASSOC); // to get the block starting address
    increment_age_blocks_L1();

    for (int i = index; i < (index + L1_ASSOC); i++)
    {
        if (L1[i].valid_bit == 1 && L1[i].tag == tag_bits )
        {
            L1[i].dirty_bit = 'D';
            L1[i].age = 0; //set age to max counter value
            return;
        }
        if (L1[i].valid_bit == 0 && (index/L1_ASSOC) == L1[i].set_number) //if miss and an empty block is available
        {
            read_L2(addr,x,y1,z1);
            L1[i].tag = tag_bits; //allocate missed block in the empty frame
            L1[i].dirty_bit = 'D';
            L1[i].valid_bit=1;
            L1[i].age = 0;
            L1_write_misses++;
            return;
        }
        
    }
    
    uint64_t max_age = 0;
    int max_age_index = 0;
    for (int i = index; i < (index + L1_ASSOC); i++)
    {
        if (L1[i].age > max_age) {
            max_age = L1[i].age;
            max_age_index = i;
        }
    }

    if (L1[max_age_index].dirty_bit == 'D')
    {
        //write_L2(L1[max_age_index].tag,x,y1,z1);
        //L1_write_backs++;
    }
    read_L2(addr,x,y1,z1);
    L1[max_age_index].tag = tag_bits;
    L1[max_age_index].dirty_bit = 'D';
    L1[max_age_index].valid_bit = 1;
    L1_write_misses++;
    L1[max_age_index].age = 0; //set age to max counter value
    return;
}


void runCacheSimulator()
{
    char action[2];
    char address[9];
    uint16_t no_of_offset_bits = log_base_2(BLOCK_SIZE);
    uint16_t no_of_index_bits_L1 = number_of_sets_L1 == 0 ? 0 : log_base_2(number_of_sets_L1);
    uint16_t no_of_index_bits_L2 = number_of_sets_L2 == 0 ? 0 : log_base_2(number_of_sets_L2);
    
    
    if (fp)
    {
        while(!feof(fp))
        {
            fscanf(fp,"%1s", action);
            //DEBUG_PRINT(("%s ", action));
            fflush(stdout);
            
            fscanf(fp,"%9s", address);
            m = strlen(address) * 4;
            int no_of_tag_bits_L1 = m - (no_of_index_bits_L1 + no_of_offset_bits ) ;
            int no_of_tag_bits_L2 = m - (no_of_index_bits_L2 + no_of_offset_bits ) ;
            
            unsigned int address_int = (int)strtol(address, NULL, 16);
            //DEBUG_PRINT(("%s ", address));
            fflush(stdout);
            if (feof(fp))
            {
                break;
            }
            
            if (strcmp(action,"r") == 0)
            {
                read_L1(address_int,no_of_offset_bits, no_of_index_bits_L1, no_of_tag_bits_L1, no_of_index_bits_L2, no_of_tag_bits_L2);
            }
            
            if (strcmp(action,"w") == 0)
            {
                write_L1(address_int, no_of_offset_bits, no_of_index_bits_L1, no_of_tag_bits_L1, no_of_index_bits_L2, no_of_tag_bits_L2 );
            }
            
        }
    }
    
}


int main(int argc, char const *argv[])
{
    /*if (argc < MIN_ARGS_REQUIRED)
     {
         printf("minimum arguments required are: 10\n" );
         //return 0;
     }*/
//
    BLOCK_SIZE = atoi(argv[1]);
    L1_SIZE = atoi(argv[2]);
    L1_ASSOC = atoi(argv[3]);
    L2_SIZE = atoi(argv[4]);
    L2_ASSOC = atoi(argv[5]);
    /*Replacement_Policy = argv[6];
    /*Inclusion_Property = argv[7]; */
        filePath = argv[8];
    //16 1024 2 0 0 LRU non-inclusive gcc_trace.txt
    //BLOCK_SIZE = 16;
    //L1_SIZE = 1024;
    //L1_ASSOC = 2;
    //L2_SIZE = 0;
    //L2_ASSOC = 0;
    /*Replacement_Policy = argv[6];
     Inclusion_Property = argv[7]; */
    //filePath = "/Users/wasyed/Desktop/gcc_trace.txt";
    number_of_blocks();
    L1 = malloc(no_of_blocks_L1 * 32);
    L2 = malloc(no_of_blocks_L2 * 32);
    number_of_sets();
    initialize_set_numbers_L1(); // also intializes the age counter
    initialize_blocks_L1();/* code */
    
    if (L2_SIZE > 0)
    {
        initialize_blocks_L2();/* code */
        initialize_set_numbers_L2();
    }
    openfile();
    runCacheSimulator();
    
    printf(" l1 reads: %d\n",L1_reads );
    printf(" l1 reads misses: %d\n",L1_read_misses );
    printf(" l1 writes: %d\n",L1_writes );
    printf(" l1 writes misses: %d\n",L1_write_misses );
    printf(" l1 writes backs: %d\n",L1_write_backs );
    
    printf(" l2 reads: %d\n",L2_reads );
    printf(" l2 reads misses: %d\n",L2_read_misses );
    printf(" l2 writes: %d\n",L2_writes );
    printf(" l2 writes misses: %d\n",L2_write_misses );
    printf(" total memory traffic: %d\n",memory_traffic );
    
    
    return 0;
}

//BLOCK_SIZE = 16;
//L1_SIZE = 1024;
//L1_ASSOC = 2;
//L2_SIZE = 0;
//L2_ASSOC = 0;
//read_L1(address_int,no_of_offset_bits, no_of_index_bits_L1, no_of_tag_bits_L1, no_of_index_bits_L2, no_of_tag_bits_L2);
//void read_L1(uint64_t addr, int x, int y, int z,int y1, int z1)
//{
//    L1_reads++;
//    unsigned mask_last_bits = (1 << x) -1; //mask to get the last x bits
//    unsigned mask = ((1<<y) -1) << (32-z); // mask to get the middle y bits starting at the end of tag bits
//    uint32_t block_offset = addr & mask_last_bits ;
//    uint16_t idx = (addr & mask) >> (32-z);
//
