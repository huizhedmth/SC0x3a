#ifndef KERNEL_H
#define KERNEL_H

#include "hardware.h"
#include "yalnix.h"
#include "list.h"
/********************** MACROS AND CONSTANT ********************/
/***************************************************************/
#ifndef NULL
#define NULL 0
#endif

#ifndef KILL
#define KILL 2012
#endif

/* page table sizes */
#define KERNEL_PAGETABLE_SIZE ( VMEM_0_SIZE >> PAGESHIFT)  
#define USER_PAGETABLE_SIZE ( VMEM_1_SIZE >> PAGESHIFT)
#define KERNEL_STACK_PAGETABLE_SIZE ( (KERNEL_STACK_LIMIT - KERNEL_STACK_BASE) >> PAGESHIFT )

/* process state constant */
#define RUNNING 0
#define READY 1
#define WAITING 2
#define BLOCKED_W 3
#define BLOCKED_R 4
#define DELAY 5
#define TERMINATED 6

#define MAX_PROC 1000
/********************** DATA STRUCTORE *************************/
/***************************************************************/
typedef struct pcb{
	int state;

	int pid;	
	int ppid;

	list children;	// list of pids

	// mark: added
	UserContext uctxt;	
	KernelContext kctxt;

	int next_heap_pn;			
	int next_stack_pn;	

	int tty_id;	/* on which it blocks */
	void* buf;
	int len;					

	struct pte u_page_table[USER_PAGETABLE_SIZE];
	struct pte k_stack_pt[KERNEL_STACK_PAGETABLE_SIZE];
	
}pcb_t;

/********************* KERNEL GLOBAL VARS **********************/
/***************************************************************/
int vmem_enabled;

int total_frames;
list free_frames;
void* kernel_brk;	
void* data_seg_limit;	/* the address right between DATA and HEAP */
void* data_seg_base;	/* the address right between DATA and TEXT */

/* process relative */
int next_pid;
list ready_queue;

/* all kinds of blocking... */
list sleep_list;/* processes that are blocked with Delay call */
list wait_list;	/* processes that are blocked with Wait call */
list io_list;	/* processes that are blocked with Tty family calls */
list dead_list;
list pipe_list;
list lock_list;
list cvar_list;
list reclaim_list;

list sem_list;
int next_sem;	// keep track of sem ids
typedef struct semaphore{
	int id;
	int count;
	list waiting;
}semaphore;

pcb_t* active_proc;
pcb_t* p_idle;
pcb_t* p_init;

/* entry indexed by pid */
pcb_t* pcb_table[MAX_PROC];

/* a copy of kernel state before leaving KernelStart
 * a process is given this kernel context and stack 
 * when first switched in
 */
KernelContext initial_kc;
struct pte initial_ks[2];

/* I/O buffer */
char receive[NUM_TERMINALS][TERMINAL_MAX_LINE];
char transmit[NUM_TERMINALS][TERMINAL_MAX_LINE];
int busy_terminal[NUM_TERMINALS];	/* avoid dup writing */
int string_len;



/* actual kernel page table in use 
 * rewrite kernel stack here for each process
 */
struct pte krnl_page_table[KERNEL_PAGETABLE_SIZE];

/******************** KERNEL FUNTIONS *********************/
/**********************************************************/
void (*int_vector[TRAP_VECTOR_SIZE])(UserContext*);

int SetKernelBrk(void* addr);
void SetKernelData(void* KernelDataStart, void* KernelDataEnd);


#endif // KERNEL_H
