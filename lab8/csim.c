#include "cachelab.h"

#include <getopt.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BIT_OF_ADDRSS	64

/* Define the a line of a set */
typedef struct {
	int lru;
	int valid;
	unsigned int tag;
} Line_t;

/* Define the struct of the set */
typedef Line_t *Set_t;

/* Define the struct of the cache */
typedef struct {
	int set_bits;
	int tag_bits;
	int block_bits;

	int set_num;
	int line_num;
	int block_num;

	int hits;
	int misses;
	int evictions;

	Set_t *sets;
} Cache_t;

/* Define the struct of the address */
typedef struct {
	unsigned int tag;
	int set_addr;
} address_t;

address_t get_addr(int addr, Cache_t *cache_sim);
void cache_init(Cache_t *cache_sim, int S, int E, int B);
int find_line(Set_t set, Cache_t *cache_sim);
void update_lru(Line_t *lines, int index, Cache_t *cache_sim);
void print_help_menu();

/* Return the address at format of address_t */
address_t get_addr(int addr, Cache_t *cache_sim){
 	address_t res;
	int i = 0;

	/* Get the bit mask */
	unsigned int set_mask = 1;
	unsigned int tag_mask = 1;

	for (i = 0; i < cache_sim->set_bits - 1; i++)
		set_mask |= (set_mask << 1);

	for (i = 0; i < cache_sim->tag_bits - 1; i++)
		tag_mask |= (tag_mask << 1);

 	res.set_addr = (addr >> cache_sim->block_bits) & set_mask;
 	res.tag = (addr >> (cache_sim->block_bits + cache_sim->set_bits)) & tag_mask;

 	return res;
}

/* Initialize the cache */
void cache_init(Cache_t *cache_sim, int s, int e, int b){
	cache_sim->hits = 0;
	cache_sim->misses = 0;
	cache_sim->evictions = 0;

	cache_sim->set_bits = s;
	cache_sim->tag_bits = BIT_OF_ADDRSS - s - b;
	cache_sim->block_bits = b;

	cache_sim->set_num = 1 << s;
	cache_sim->line_num = e;
	cache_sim->block_num = 1 << b;


	cache_sim->sets = (Set_t *)malloc(sizeof(Set_t) * cache_sim->set_num);

	int i, j = 0;

	/* Initialize every set */
	for (i = 0; i < cache_sim->set_num; i++) {
		cache_sim->sets[i] = (Line_t *)malloc(sizeof(Line_t) * cache_sim->line_num);
		/* Initialize every line of a set */
		for (j = 0; j < cache_sim->line_num; j++) {
			cache_sim->sets[i][j].lru = -1;
			cache_sim->sets[i][j].valid = 0;
			cache_sim->sets[i][j].tag = 0;
		}
	}
}

/* Find the line which has the largest lru */
int find_line(Set_t set, Cache_t *cache_sim) {
	int lru = 0;
	int index = 0; /* The index of line which has the largest lru */
	int i = 0;

	for (i = 0; i < cache_sim->line_num; i++) {
		if (set[i].lru > lru) {
			lru = set[i].lru;
			index = i;
		}
	}
	return index;
}

/* Update the lru of a line after having access to it 
 * The index represent the index of line which user have
 * just visited
 */
void update_lru(Line_t *lines, int index, Cache_t *cache_sim) {
	lines[index].lru = 0;
	int i = 0;
	for (i = 0; i < cache_sim->line_num; i++) {
		if (i != index && lines[i].valid == 1)
			lines[i].lru++;
	}
}

/* Beacause the operation of load and store have the same effect,
 * so it can be merged to one funcion
 */
void load_store(address_t addr, Cache_t *cache_sim, int verbose) {
	Set_t set = cache_sim->sets[addr.set_addr];
	unsigned int tag = addr.tag;
	int i = 0;
	/* Find the line which has corresponding tag */
	for (i = 0; i < cache_sim->line_num; i++) {
		if (set[i].valid == 1 && set[i].tag == tag) {
			update_lru(set, i, cache_sim);
			cache_sim->hits++;
			
			if (verbose == 1) 
				printf("hit ");

			return;
		}
	}
	
	/* If none of line has the corresponding tag, the status is miss */
	cache_sim->misses++;

	if (verbose == 1)
		printf("miss ");

	/* Determine if there exists empty line */
	for (i = 0; i < cache_sim->line_num; i++) {
		if (set[i].valid == 0) {
			set[i].tag = tag;
			set[i].valid = 1;
			update_lru(set, i, cache_sim);
			return;
		}
	}
			
	/* If it doesn't exist empty find, find the line which 
	 * has largest lru to evict it */
	int index = find_line(set, cache_sim);
	set[index].tag = tag;
	update_lru(set, index, cache_sim);
	cache_sim->evictions++;
	if (verbose == 1)
		printf("eviction ");
	

	return;
}


void modify(address_t addr, Cache_t *cache_sim, int verbose) {
	load_store(addr, cache_sim, verbose);
	load_store(addr, cache_sim, verbose);
}

void print_help_menu(){
	printf("\n\nUsage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
	printf("-h:             Optional help flag that prints usage info\n");
	printf("-v:             Optional verbose flag that displays trace info\n");
	printf("-s <s>:         Number of set index bits(S = 2^s is the number of sets)\n");
	printf("-E <E>:         Associativity (number of lines per set)\n");
	printf("-b <b>:         Number of block bit(b = 2^b is the block size)\n");
	printf("-t <tracefile>: Name of the valgrind trace to replay\n\n\n");
}

int main(int argc, char ** argv) {
	int s, E, b, verbose = 0;
	char *filename = NULL;
	int c = getopt(argc, argv, "hvs:E:b:t");

	if (c == -1){
		print_help_menu();
		return -1;
	}

	do{
		switch(c) {
            case 'v':
                verbose = 1;
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
                filename = optarg;
                break;
            default:
                print_help_menu();
                return -1;
        }
    }while((c = getopt(argc, argv, "hvs:E:b:t:")) != -1);
    Cache_t *cache_sim = (Cache_t *)malloc(sizeof(Cache_t));
	
    cache_init(cache_sim, s, E, b);

	int addr;
	int block_size = 0;
	char opt[2];
	address_t addr_s;

	FILE *file = fopen(filename, "r");

    while(fscanf(file,"%s %x,%d",opt,&addr,&block_size) != EOF) {
    	if(verbose == 1)
			printf("%s %x,%d ",opt,addr,block_size);
		
		if (opt[0] == 'I') {
			if (verbose == 1)
				printf("\n");
			continue;
		}

    	addr_s = get_addr(addr, cache_sim);

    	if (opt[0] == 'M')
    		modify(addr_s, cache_sim, verbose);

		if (opt[0] == 'L')
    		load_store(addr_s, cache_sim, verbose);

		if (opt[0] == 'S')
    		load_store(addr_s, cache_sim, verbose);

    	if (verbose == 1)
    		printf("\n");
	}
	printSummary(cache_sim->hits, cache_sim->misses, cache_sim->evictions);
    return 0;
}
