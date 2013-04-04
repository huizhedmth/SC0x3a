#include "hardware.h"
#include "yalnix.h"

#include "trap.h"
#include "kernel.h"
#include "scheduler.h"
#include "loadprogram.h"

/* helper functions in KernelStart */

void init_int_vector();
void gather_free_frames();
void enable_vmem();
void init_queues();

/* return pointer to a process from its pid, or NULL if error occurs */
pcb_t* get_pcb(int pid);

/* init two special processes */
void init_init(UserContext*, char* cmd_args[]); /* set up init's pcb and etc. */
void init_idle(UserContext*, char* cmd_args[]);	

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt)
{
	int i;
	int pfn;
	void *dst, *src;

	/* we first initialize the kernel by
	 * init vector, register frames, set page table, enable vmem, along with
	 * global vars like queues and etc.
	 */

	TracePrintf(2, "KernelStart: entered.\n");
	total_frames = pmem_size >> PAGESHIFT;

	next_pid = 0;
	next_sem = 0;
	vmem_enabled = 0;	
	for (i = 0; i < NUM_TERMINALS; i++)
		busy_terminal[i] = 0;

	init_queues();

	init_int_vector();

	gather_free_frames();

	enable_vmem();

	/* we have finished initializing the kernel
	 * now we take care of initialization of idle and init
	 * including their pcb, user page table and kernel stack
	 */ 

	init_init(uctxt, cmd_args);	// pid 0
	init_idle(uctxt, cmd_args);	// pid 1

	// add idle to ready_queue
	list_insert(&ready_queue, ready_queue.len+1, (void*)p_idle);

	/* set active_proc before save_first_kernel
	 * note what the following memcpy does
	 */
	active_proc = p_init;

	/* get a copy of fresh new kernel stack & kernel context
	 * every time a process gets switched in for the FIRST time
	 * it is assigned with initial kernel stack and context
	 * so it comes exactly after the following KernelContextSwitch
	 * then memcpy properly sets user context according to active_proc
	 * to ensure the switched-in process could run
	 */
	KernelContextSwitch(save_initial_kernel, NULL, NULL);	

	memcpy(uctxt, &(active_proc->uctxt), sizeof(UserContext));
	WriteRegister(REG_PTBR1, (unsigned int)(active_proc->u_page_table));
	WriteRegister(REG_PTLR1, (unsigned int)(USER_PAGETABLE_SIZE));
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

	TracePrintf(2, "KernelStart: returning...\n");

}  // end of KernelStart

void SetKernelData(void* KernelDataStart, void* KernelDataEnd)
{
	TracePrintf(2, "SetKernelData: entered.\n");
	data_seg_base = KernelDataStart;
	data_seg_limit = kernel_brk = KernelDataEnd;
	TracePrintf(2, "SetKernelData: returning.\n");
}

int SetKernelBrk(void* addr)
{
	int pn_start, pn_end;	// new page indice 
	int i;
	int pfn;

	TracePrintf(2, "SetKernelBrk: entered.\n");

	// vmem disabled
	if (vmem_enabled == 0){		
		if (addr > kernel_brk)	
			kernel_brk = addr;
		TracePrintf(2, "SetKernelBrk: returning.\n");
		return 0;
	}

	// vmem enabled
	else {				
	  	if (addr > kernel_brk) {	
			if ((int)addr >= KERNEL_STACK_BASE) {	// out of vmem
				TracePrintf(0, "SetKernelBrk: error: out of vmem\n");
				TracePrintf(0, "SetKernelBrk: pid: %d\n", active_proc->pid);
				return ERROR;
			}	

			// calculate page numbers
			pn_end = (int)(addr) >> PAGESHIFT;
			pn_start = (int)kernel_brk >> PAGESHIFT;

			// find free frames
			for(i = pn_start; i < pn_end; i++){
				pfn = grab_a_frame();
				if (pfn == 0)	{  // fail
					TracePrintf(0, "SetKernelBrk: error: out of pmem\n");
					return ERROR;
				}
				else {
					krnl_page_table[i].valid = 1;
					krnl_page_table[i].prot = PROT_WRITE | PROT_READ;
					krnl_page_table[i].pfn = pfn;	
				} 
			} 
			kernel_brk = addr;	// set new brk
		}  	
		TracePrintf(2, "SetKernelBrk: returning.\n");	
		return 0;	  		
	} 
	
}

/***********************************************************************/
void init_int_vector()
{
	int i;

	TracePrintf(2, "init_int_vector: entered.\n");	
	int_vector[TRAP_KERNEL] = trap_kernel_hdl;
	int_vector[TRAP_CLOCK] = trap_clock_hdl;
	int_vector[TRAP_ILLEGAL] = trap_illegal_hdl;
	int_vector[TRAP_MEMORY] = trap_memory_hdl;
	int_vector[TRAP_MATH] = trap_math_hdl;
	int_vector[TRAP_TTY_RECEIVE] = trap_tty_receive_hdl;
	int_vector[TRAP_TTY_TRANSMIT] = trap_tty_transmit_hdl;
	int_vector[TRAP_DISK] = trap_disk_hdl;
	for (i = TRAP_DISK+1; i < TRAP_VECTOR_SIZE; i++)
		int_vector[i] = trap_undefined_hdl;
	WriteRegister(REG_VECTOR_BASE, (unsigned int)int_vector);
	TracePrintf(2, "init_int_vector: returning.\n");	
}

void gather_free_frames()
{
	int i;

	TracePrintf(2, "gather_free_frames: entered.\n");
	free_frames.len = 0;
	free_frames.head = (node*)malloc(sizeof(node));
	if (free_frames.head == NULL)	{	
	  	TracePrintf(0, "KernelStart: not enough memory\n");
		return;
	}

	// first part: from top most down to kernel stack limit 
	for (i = total_frames-1; i > (VMEM_0_LIMIT >> PAGESHIFT)-1; i--){
		if (list_insert(&free_frames, free_frames.len+1, (void *)i) ) {
			TracePrintf(0, "KernelStart: internal error in list\n");
			return;
		}
	}

	// second part: from KERNAL_STACK_BASE down to kernel_brk 
	for (i = (KERNEL_STACK_BASE >> PAGESHIFT)-1; 
		i > (UP_TO_PAGE((int)kernel_brk) >> PAGESHIFT)-1; i--){
	  	if (list_insert(&free_frames, free_frames.len+1, (void *)i) ) {
			TracePrintf(0, "KernelStart: internal error in list\n");
			return;
		}
	}
	TracePrintf(2, "gather_free_frames: returning...\n");
}

void enable_vmem()
{
	int i;

	TracePrintf(2, "enable_vmem: entered.\n");

	// setup the mapping from vpn->pfn and enable virtual memory
		
	for (i = VMEM_0_BASE >> PAGESHIFT; i < VMEM_0_LIMIT >> PAGESHIFT; i ++)
	krnl_page_table[i].valid = 0;

	// kernel Text  
	for (i = VMEM_0_BASE >> PAGESHIFT; i < (int)data_seg_base >> PAGESHIFT; i ++){
		krnl_page_table[i].valid = 1;
		krnl_page_table[i].prot = PROT_READ | PROT_EXEC;
		krnl_page_table[i].pfn = i;	
	}

	// kernel Data 
        for (i = (int)data_seg_base >> PAGESHIFT; i < (int)data_seg_limit >> PAGESHIFT; i++){
	        krnl_page_table[i].valid = 1;
		krnl_page_table[i].prot = PROT_READ | PROT_WRITE;
		krnl_page_table[i].pfn = i;		
	
	}

	// kernel Heap
        for (i = (int)data_seg_limit >> PAGESHIFT; i < (int)kernel_brk >> PAGESHIFT; i++){
		krnl_page_table[i].valid = 1;
		krnl_page_table[i].prot = PROT_READ | PROT_WRITE;
   		krnl_page_table[i].pfn = i;	
	}

	// kernel Stack 
	for (i = KERNEL_STACK_BASE >> PAGESHIFT; i < KERNEL_STACK_LIMIT >> PAGESHIFT; i++){
		krnl_page_table[i].valid = 1;
		krnl_page_table[i].prot = PROT_READ | PROT_WRITE;
		krnl_page_table[i].pfn = i;	
	}

	// tell MMU where to find page table 
	WriteRegister(REG_PTBR0, (unsigned int)(krnl_page_table));
	WriteRegister(REG_PTLR0, (unsigned int)(KERNEL_PAGETABLE_SIZE));	

	WriteRegister(REG_VM_ENABLE, 1);
	vmem_enabled = 1;

	TracePrintf(2, "enable_vmem: returning...\n");
}

void init_queues()
{
	TracePrintf(2, "init_queues: entered.\n");
	ready_queue.len = 0;
	ready_queue.head = (node*)malloc(sizeof(node));
	if (ready_queue.head == NULL) {
		TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}

	sleep_list.len = 0;
	sleep_list.head = (node*)malloc(sizeof(node));
	if (sleep_list.head == NULL) {
		TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}
	
	dead_list.len = 0;
	dead_list.head = (node*)malloc(sizeof(node));
	if (dead_list.head == NULL) {
		TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}
	
	wait_list.len = 0;
	wait_list.head = (node*)malloc(sizeof(node));
	if (wait_list.head == NULL) {
		TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}

	io_list.len = 0;
	io_list.head = (node*)malloc(sizeof(node));
	if (io_list.head == NULL) {
		TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}
	
	pipe_list.len = 0;
	pipe_list.head = (node*)malloc(sizeof(node));
	if(pipe_list.head == NULL){
	  	TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}

	lock_list.len = 0;
	lock_list.head = (node*)malloc(sizeof(node));
	if(lock_list.head == NULL){
	  	TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}
	
	cvar_list.len = 0;
	cvar_list.head = (node*)malloc(sizeof(node));
	if(cvar_list.head == NULL){
	  	TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}
	
	sem_list.len = 0;
	sem_list.head = (node*)malloc(sizeof(node));
	if(sem_list.head == NULL){
	  	TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}

	//one list for bookkeeping pipe, lock, cvar
	reclaim_list.len = 0;
	reclaim_list.head = (node*)malloc(sizeof(node));
	if(reclaim_list.head == NULL){
	  	TracePrintf(0, "init_queues: error: not enough mem.\n");
		return;
	}

	TracePrintf(2, "init_queues: returning...\n");
}


pcb_t* get_pcb(int pid)
{
	TracePrintf(2, "get_pcb: entered.\n");
	if (pid > MAX_PROC-1){
		TracePrintf(0, "get_pcb: error: bad argument. pid must be less than %d\n", MAX_PROC-1);
		return NULL;
	}
	return pcb_table[pid];
	TracePrintf(2, "get_pcb: returning...\n");
}

void init_idle(UserContext* uctxt, char* cmd_args[])
{
	int i, pfn;
	TracePrintf(2, "init_idle: entered.\n");

	// create a pcb for init process
	if ( NULL == (p_idle = (pcb_t*)malloc(sizeof(pcb_t))) ) {
		TracePrintf(0, "init_idle: error creating pcb.\n");
		return;
	}

	memcpy(&(p_idle->uctxt), uctxt, sizeof(UserContext));
	p_idle->state = READY;
	p_idle->pid = next_pid++;
	pcb_table[p_idle->pid] = p_idle;

	// about its parent init...
	p_idle->ppid = -1;
	//list_insert(&(p_init->children), (p_init->children).len+1, (void*)p_idle->pid);

	p_idle->children.len = 0;
	p_idle->children.head = (node*)malloc(sizeof(node));
	if (p_idle->children.head == NULL){
		TracePrintf(0, "init_idle: error: not enough mem.\n");
	}

	for (i = 0; i < 2; i++){
		p_idle->k_stack_pt[i].pfn = 0;
	}
	memset(p_idle->u_page_table, 0, USER_PAGETABLE_SIZE*sizeof(struct pte)); 
	WriteRegister(REG_PTBR1, (unsigned int)(p_idle->u_page_table));
	WriteRegister(REG_PTLR1, (unsigned int)(USER_PAGETABLE_SIZE));
	load_program("idle", &cmd_args[1], p_idle);	// mark: cmd_args
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	TracePrintf(2, "init_idle: returning...\n");	
}

void init_init(UserContext* uctxt, char* cmd_args[])
{
	int i, pfn, rc;
	// create a pcb for init process

	TracePrintf(2, "init_init: entered.\n");
	if ( NULL == (p_init = (pcb_t*)malloc(sizeof(pcb_t))) ) {
		TracePrintf(0, "init_init: error creating pcb.\n");
		return;
	}

	memcpy(&(p_init->uctxt), uctxt, sizeof(UserContext));
	p_init->state = RUNNING;
	p_init->pid = next_pid++;
	pcb_table[p_init->pid] = p_init;

	p_init->ppid = -1;	// it's THE parent
	p_init->children.len = 0;
	p_init->children.head = (node*)malloc(sizeof(node));
	if (p_init->children.head == NULL){
		TracePrintf(0, "init_init: error: not enough mem.\n");
	}

	// allocate init's kernel stack 
	for (i = 0; i < 2; i++){
		p_init->k_stack_pt[i].valid = 1;
		p_init->k_stack_pt[i].prot = PROT_READ | PROT_WRITE;
		if (0 == (pfn = grab_a_frame())){
			TracePrintf(0, "init_init: error: not enough mem\n");
			return;
		}
		p_init->k_stack_pt[i].pfn = pfn;
		TracePrintf(3, "init_init: k_stack pfn: %d\n", pfn);
	}


	memset(p_init->u_page_table, 0, USER_PAGETABLE_SIZE*sizeof(struct pte)); 	// don't forget this. ensure every local variable gets INITIALIZED!
	WriteRegister(REG_PTBR1, (unsigned int)(p_init->u_page_table));
	WriteRegister(REG_PTLR1, (unsigned int)(USER_PAGETABLE_SIZE));
	rc = load_program(cmd_args[0], &cmd_args[1], p_init);	// mark: cmd_args
	if (rc != 0) {
		TracePrintf(0, "init_init: failed loading init process\n");
	}
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);	

	TracePrintf(2, "init_init: returning...\n");
}
	
