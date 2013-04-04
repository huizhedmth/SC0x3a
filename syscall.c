#include "hardware.h"	

#include "kernel.h"	
#include "syscall.h"
#include "mem.h"	
#include "scheduler.h"

int sys_Brk(UserContext* u_context)
{
	int pfn, pn_end, pn_start;
	int i;
	void* addr;

	TracePrintf(2,"sys_brk: entered.\n");

	addr = (void*)(*u_context).regs[0];

	TracePrintf(3,"sys_brk: next heap page number: %d\n", active_proc->next_heap_pn);

	pn_end = (UP_TO_PAGE(addr)-VMEM_1_BASE) >> PAGESHIFT;
	pn_start = active_proc->next_heap_pn;

	TracePrintf(3,"sys_brk: pn_end:%d\n", pn_end);
	for (i = pn_start; i < pn_end; i++) {
		pfn = grab_a_frame();
		if (pfn == 0){	// failed
			TracePrintf(0, "sys_brk: error in sys_brk: not enough memory.\n");
			return ERROR;
		}
		active_proc->u_page_table[i].valid = 1;
		active_proc->u_page_table[i].prot = PROT_READ | PROT_WRITE;
		active_proc->u_page_table[i].pfn = pfn;
	}
	active_proc->next_heap_pn = pn_end;
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	TracePrintf(2, "sys_brk: returning...\n");
	return 0;
}

int sys_Fork(UserContext* u_context)
{
	int i, pfn;
	int rc;
	pcb_t* p_child;
	void *dst, *src;


	TracePrintf(2,"sys_Fork: entered.\n");
	
	// create a pcb for the process to be forked
	if ( NULL == (p_child = (pcb_t*)malloc(sizeof(pcb_t))) ) {
		TracePrintf(0, "sys_Fork: error creating pcb.\n");
		return ERROR;
	}
	
	// set up the new pcb as a copy of the current pcb

	p_child->state = READY;
	p_child->ppid = active_proc->pid;
	p_child->pid = next_pid++;
	pcb_table[p_child->pid] = p_child;
	list_insert(&(active_proc->children), (active_proc->children).len+1, (void*)p_child->pid);

	p_child->children.len = 0; 
	if (NULL == (p_child->children.head = (node*)malloc(sizeof(node)))){
		TracePrintf(0, "sys_Fork: error: not enough mem.\n");
		return ERROR;
	}		

	// user context, kernel context & kernel stack
	memcpy(&(p_child->uctxt), u_context, sizeof(UserContext));
	(p_child->uctxt).regs[0] = 0;
	rc = KernelContextSwitch(init_proc_kernel, p_child, NULL);
	if (rc == ERROR) {
		TracePrintf(0, "sys_Fork: KernelContextSwitch returned ERROR.\n");
	}

	// copy user page table
	memset(p_child->u_page_table, 0, USER_PAGETABLE_SIZE*sizeof(struct pte)); 
	for(i = 0; i < USER_PAGETABLE_SIZE; i++){
		if (active_proc->u_page_table[i].valid == 1)	{// existing page, so copy it
			p_child->u_page_table[i].valid = 1;
			p_child->u_page_table[i].prot = active_proc->u_page_table[i].prot;
			if (0 == (pfn = grab_a_frame())){
				TracePrintf(0, "sys_Fork: error: not enough mem\n");
				return ERROR;
			}
			p_child->u_page_table[i].pfn = pfn;
			dst = (void*) ((p_child->u_page_table[i].pfn) << PAGESHIFT);
			src = (void*) ((active_proc->u_page_table[i].pfn) << PAGESHIFT);
			memcpy(dst, src, PAGESIZE);
		}
	}

	// other bookkeeping
	p_child->next_heap_pn = active_proc->next_heap_pn;
	p_child->next_stack_pn = active_proc->next_stack_pn;

	// add child to ready queue
	list_insert(&ready_queue, ready_queue.len+1, (void*)p_child);

	TracePrintf(2,"sys_Fork: returning...\n");
	return p_child->pid;
}

int sys_Exec(UserContext* u_context)
{
	int i, rc;
	pcb_t* tmp;
	void* addr;
	int page_num;

	TracePrintf(2, "sys_Exec: entered.\n");
	//TracePrintf(0, "sys_Exec: u_context->regs[1]: %p\n", u_context->regs[1]);
	//TracePrintf(0, "sys_Exec: before load_program");
	
	// sanity check

	addr = (void*)u_context->regs[1];
	page_num = (DOWN_TO_PAGE(addr) - VMEM_1_BASE) >> PAGESHIFT;

	if(addr == NULL) {
		TracePrintf(0, "sys_Exec: error: bad argument 2\n");
		return ERROR;
	}

	if ((int)addr < VMEM_1_BASE){
		TracePrintf(0, "sys_Exec: error: cannot access kernel memory!\n");
		return ERROR;
	}

	if ( (page_num >= active_proc->next_heap_pn) && 
		(page_num <= active_proc->next_stack_pn) ) {
		TracePrintf(0, "sys_Exec: error: illegal address!\n");
		return ERROR;	 
	}

	// load program
	rc = load_program(u_context->regs[0], u_context->regs[1], active_proc);
	TracePrintf(3, "sys_Exec: load_program returned with: %d\n", rc);
	if (rc == ERROR){
		TracePrintf(0, "sys_Exec: error: executable not found.\n");
		return ERROR;
	}

	// modify current user context and return to initial kernel state(end of KernelStart)
	memcpy(u_context, &(active_proc->uctxt), sizeof(UserContext));
	rc = KernelContextSwitch(return_initial_state, NULL, NULL);

	if (rc == ERROR) {
		TracePrintf(0, "sys_Exec: KernelContextSwitch returned ERROR.\n");
	}

	// if KernelContextSwitch succeeded, sys_Exec will NOT return
	TracePrintf(2,"sys_Exec: something is wrong, returning...\n");
	return ERROR;
}

int sys_Wait(UserContext* u_context)
{
	int* status_ptr;
	int child_pid;
	int i=1;
	dead_info* d;
	node *curr;
	void* addr;
	int page_num;

	TracePrintf(2,"sys_wait: entered.\n");

	// sanity check
	addr = (void*)u_context->regs[0];
	page_num = (DOWN_TO_PAGE(addr) - VMEM_1_BASE) >> PAGESHIFT;

	if (addr == NULL){
		TracePrintf(0, "sys_wait: error: bad argument!\n");
		return ERROR;
	}

	if ((int)addr < VMEM_1_BASE){
		TracePrintf(0, "sys_wait: error: cannot access kernel memory!\n");
		return ERROR;
	}
	
	if ( (page_num >= active_proc->next_heap_pn) && 
		(page_num <= active_proc->next_stack_pn) ) {
		TracePrintf(0, "sys_wait: error: illegal address!\n");
		return ERROR;	 
	}
	
	
	status_ptr = (int* )u_context->regs[0];
	TracePrintf(2,"sys_wait: status acquired: %d\n", *status_ptr);

	//if process have no remaining child return ERROR
	TracePrintf(2,"sys_wait: child list length: %d\n", (active_proc->children).len);
	if(active_proc->children.len <=0){
		TracePrintf(2,"The parent has no child\n");
		return ERROR;
	}
	
	//go through the dead process list
	curr = list_index(&dead_list,1);
	while(curr!=NULL){
		d =(dead_info* )curr->data;

		//find a exited child
		if(active_proc->pid==d->ppid)
		{			
			*status_ptr = d->status;
			child_pid = d->pid;
			free(d);
			free(list_remove(&dead_list,i));	        
			TracePrintf(2,"sys_wait: return with childpid: %d\n",child_pid);
			return child_pid;
		}
		curr = curr->next;
		i++;
	}

	TracePrintf(2,"sys_wait: no child find\n");
	
	list_insert(&wait_list,wait_list.len+1,(void*)active_proc);
	
	if(normal_schedule(u_context)!=0){
		TracePrintf(0,"sys_Wait: normal_schedule error\n");
		return ERROR;
	}
	
	
	//if being wake up by exit
	//go through the dead process list
	curr = list_index(&dead_list,1);
	while(curr!=NULL){
		d =(dead_info* )curr->data;

		//find a exited child
		if(active_proc->pid==d->ppid)
		{			
			*status_ptr = d->status;
			child_pid = d->pid;
			free(d);
			free(list_remove(&dead_list,i));	        
			TracePrintf(2,"sys_wait: return with childpid: %d\n",child_pid);
			return child_pid;
		}
		curr = curr->next;
		i++;
	}
	
	//if still not found, return ERROR
	TracePrintf(0,"sys_wait: no children to wait\n");
	return ERROR;
	
}

int sys_Exit(UserContext* u_context, int mode)
{
	//mode 0 for normal exit,mode 1 for illegal abort
  	int i,j,n,pfn,rc;
	int status;
	pcb_t *p, *p1;
	node *curr, *rc1;
	pcb_t *wait, *tmp;

	TracePrintf(2, "sys_Exit: entered in mode %d.\n", mode);
	
	if(mode==0) 
		status = u_context->regs[0];
	else
		status = ERROR;
	
	//remove itself from the "parent" tag of each of its child
	node* cur  = list_index(&(active_proc->children),1);
	while(cur != NULL)
	{	  
		p = (pcb_t*)get_pcb((int)(cur->data));
		p->ppid = -1;	
		cur = cur->next;	
	}

	//check its parent
	if(active_proc->ppid != -1){
		//insert into deadlist
		rc = insert_dead_list(active_proc->ppid,active_proc->pid, status);
		if(rc!=0){
			TracePrintf(0,"sys_exit: error: out of kernel memory.\n");
			return ERROR;
		}
		
		//remove it from its parent's children
		p1 = (pcb_t*)get_pcb(active_proc->ppid);
		for(n=1;n<=p1->children.len;n++){			
			if((int)(list_index(&(p1->children),n)->data) == active_proc->pid){
				free(list_remove(&(p1->children),n));
				break;
			}
		}
		
	}

	

	//free all frames used in region 1
	for(i = (VMEM_1_BASE - VMEM_1_BASE) >> PAGESHIFT;
		i < (VMEM_1_LIMIT - VMEM_1_BASE) >> PAGESHIFT;
		i++)
	{
		if (active_proc->u_page_table[i].valid == 1) {		
			active_proc->u_page_table[i].valid = 0;
			pfn = active_proc->u_page_table[i].pfn;
			
			list_insert(&free_frames, 1, (void*)pfn);// add to free list		
			active_proc->u_page_table[i].prot = 0;
		}		
	}
	
	//free kernel stack
	for (i = 0; i < KERNEL_STACK_PAGETABLE_SIZE; i++)
	{
		if (active_proc->k_stack_pt[i].valid == 1) {		
			active_proc->k_stack_pt[i].valid = 0;
			pfn = active_proc->k_stack_pt[i].pfn;
			
			list_insert(&free_frames, 1, (void*)pfn);// add to free list		
			active_proc->k_stack_pt[i].prot = 0;
		}		
	}
	
	//all process waiting for this 
	curr = list_index(&wait_list,1);
	j=1;

	while(curr!=NULL){
	  	wait = (pcb_t*)(curr->data); 
		TracePrintf(2,"sys_exit: pcb: %d, %d\n",wait->pid,active_proc->ppid);
		if(wait->pid == active_proc->ppid){
			rc1 = list_remove(&wait_list,j);	
			if(rc1!=NULL){
				tmp = (pcb_t*)(rc1->data);
				free(rc1);
			}else{
				TracePrintf(0,"sys_Exit: list_remove error\n");
				return ERROR;
			}
    			list_insert(&ready_queue, ready_queue.len+1,(void*)tmp);
	    		break;
	  	}
	  	curr = curr->next;
	  	j++;

	}
	
	active_proc = NULL;
	if(normal_schedule(u_context)!=0){
		TracePrintf(0,"sys_Exit: normal_schedule error\n");
		return ERROR;
	}
	TracePrintf(2, "sys_Exit: finished.\n");
	return 0;

}


int sys_Getpid()
{
 	return active_proc->pid;
}

int sys_Delay(UserContext* u_context)
{	
 	int rc;
	pcb_t* next;
	int clock_ticks = u_context->regs[0];

	TracePrintf(2,"sys_Delay: entered.\n");
	
	TracePrintf(2,"sys_Delay: clock ticks acquired: %d.\n",clock_ticks);

	// check argument
	if(clock_ticks < 0 ){
	        TracePrintf(0,"sys_Delay: error: bad argument\n");
		return ERROR;		// mark: sys_Delay has return value type of INT
	}

	if (clock_ticks == 0)
		return 0;

	if(insert_sleep_list(active_proc, clock_ticks) != 0){
		TracePrintf(0,"sys_Delay: error: out of kernel memory.\n");
		return ERROR;
		// mark: what do we do if error happened? we are not gonna continue this context switch
		// and modification on queues, right?
	}
	TracePrintf(3,"sys_Delay: switching to another process...\n");
	
	// mark: we should check... although logically idle is always in the ready queue
	// when it is not runing
	if(normal_schedule(u_context)!=0){
		TracePrintf(0,"sys_Delay: normal_schedule error\n");
		return ERROR;
	}
	TracePrintf(2,"sys_Delay: returning.\n");
	return 0;
	
}

int sys_TtyRead(UserContext* u_context)
{
	pcb_t* next;
	int tty_id = u_context->regs[0];
	int arg1 = (int)u_context->regs[2];
	int page_num;
	TracePrintf(2,"sys_TtyRead: entered.\n");

	TracePrintf(3,"sys_TtyRead: tty_id: %d, buf: %p, len: %d.\n", u_context->regs[0], \
u_context->regs[1], u_context->regs[2]);

	// sanity check 
	page_num = (DOWN_TO_PAGE(u_context->regs[1]) - VMEM_1_BASE) >> PAGESHIFT; 

	if ( tty_id < 0 || tty_id > NUM_TERMINALS ){
		TracePrintf(0, "sys_TtyRead: error: illegal terminal.\n");
		return ERROR;	
	}
 
	if (arg1 < 0 ) {
		TracePrintf(0, "sys_TtyRead: error: illegal number of bytes specified.\n");
		return ERROR;
	}

	if ( (page_num >= active_proc->next_heap_pn) && 
		(page_num <= active_proc->next_stack_pn) ){
		TracePrintf(0, "sys_TtyRead: error: be careful with your pointer!\n");
		return ERROR;
	}

	if (u_context->regs[1] < VMEM_1_BASE){
		TracePrintf(0, "sys_TtyRead: error: not permitted to access the kernel!\n");
		return ERROR;
	}


	// bookkeeping
	active_proc->tty_id = u_context->regs[0];
	active_proc->buf = (void*)u_context->regs[1];
	active_proc->len = u_context->regs[2];
	 
	TracePrintf(3,"sys_TtyRead: blocking process %d...\n", active_proc->pid);

	active_proc->state = BLOCKED_R;
	if (0 != ( list_insert(&io_list, io_list.len+1, (void*)(active_proc)))){
		TracePrintf(0,"sys_TtyRead: error: out of mem.\n");
		return ERROR;
	}

	TracePrintf(3,"sys_TtyRead: switching to another process...\n");
	if(normal_schedule(u_context)!=0){
		TracePrintf(0,"sys_TtyRead: normal_schedule error\n");
		return ERROR;
	}

	TracePrintf(1,"sys_TtyRead: process %d is back.\n", active_proc->pid);

	// now it tries to get info from kernel global vars that should have hold those info
	active_proc->len = string_len;
	memcpy(active_proc->buf, receive[tty_id], string_len);

//	TracePrintf(3, "sys_TtyRead: it got %d bytes.\n", active_proc->len);
//	TracePrintf(3, "sys_TtyRead: its buf begins at: %p\n", active_proc->buf);
//	TracePrintf(3, "sys_TtyRead: the string is: %s\n", active_proc->buf);
	
	TracePrintf(2,"sys_TtyRead: returning.\n");

	return active_proc->len;

}


int sys_TtyWrite(UserContext* u_context)
{
	pcb_t* next;
	int n_loops;
	void* start;
	int remain_bytes;
	int tty_id;
	int page_num;

	TracePrintf(2,"sys_TtyWrite: entered.\n");

	TracePrintf(3,"sys_TtyWrite: tty_id: %d, buf: %p, len: %d.\n", u_context->regs[0], \
u_context->regs[1], u_context->regs[2]);
	
	
	tty_id = u_context->regs[0];
	n_loops = ((u_context->regs[2]-1) / TERMINAL_MAX_LINE)+1;
	start = (void*)u_context->regs[1];
	remain_bytes = u_context->regs[2];

	// sanity check 

	if ( tty_id < 0 || tty_id > NUM_TERMINALS ){
		TracePrintf(0, "sys_TtyWrite: error: illegal terminal.\n");
		return ERROR;	
	}
 
	if ( (int)u_context->regs[2] < 0 ) {
		TracePrintf(0, "sys_TtyWrite: error: illegal number of bytes specified.\n");
		return ERROR;
	}

	if ( u_context->regs[1] < VMEM_1_BASE ){
		TracePrintf(0, "sys_TtyWrite: error: you are not permitted to access the kernel!\n");
		return ERROR;
	}

	page_num = DOWN_TO_PAGE( (u_context->regs[1]) - VMEM_1_BASE) >> PAGESHIFT; 
	
	if ( (page_num >= active_proc->next_heap_pn) && 
		(page_num <= active_proc->next_stack_pn) ){
		TracePrintf(0, "sys_TtyRead: error: be careful with your pointer!\n");
		return ERROR;
	}
	
	while(remain_bytes > 0){
				

		// mark: modified 11/12

		while(busy_terminal[tty_id] == 1){ // this terminal busy, switch to another process
			//TracePrintf(0,"sys_TtyWrite: mark 1\n");
			TracePrintf(3,"sys_TtyWrite: blocking process %d...\n", active_proc->pid);
			// bookkeeping
			active_proc->tty_id = u_context->regs[0];
			active_proc->state = BLOCKED_W;
			if (0 != ( list_insert(&io_list, io_list.len+1, (void*)(active_proc)))){
				TracePrintf(0,"sys_TtyWrite: error: out of mem.\n");
				return ERROR;
			}

			TracePrintf(3,"sys_TtyWrite: switching to another process...\n");
			if(normal_schedule(u_context)!=0){
					TracePrintf(0,"normal_schedule error\n");
					return ERROR;
			}
		}
		TracePrintf(3,"sys_TtyWrite: setting busy flag...\n");
		busy_terminal[tty_id] = 1;
	
		// end modification


		if (remain_bytes > TERMINAL_MAX_LINE) {
			remain_bytes -= TERMINAL_MAX_LINE;
			TtyTransmit(tty_id, (void*)u_context->regs[1], TERMINAL_MAX_LINE);
		}else{
			TtyTransmit(tty_id, (void*)u_context->regs[1], remain_bytes);
			remain_bytes = 0;
		}
			
		TracePrintf(3,"sys_TtyWrite: blocking process %d...\n", active_proc->pid);
		// bookkeeping
		active_proc->tty_id = u_context->regs[0];
		active_proc->state = BLOCKED_W;
		if (0 != ( list_insert(&io_list, io_list.len+1, (void*)(active_proc)))){
			TracePrintf(0,"sys_TtyWrite: error: out of mem.\n");
			return ERROR;
		}

		TracePrintf(3,"sys_TtyWrite: switching to another process...\n");
		if(normal_schedule(u_context)!=0){
			TracePrintf(0,"sys_TtyWrite: normal_schedule error\n");
			return ERROR;
		}	
		TracePrintf(3,"sys_TtyWrite: reach end of while...\n");
	}


	TracePrintf(2,"sys_TtyWrite: returning.\n");
	return u_context->regs[2];
}

//pipe & lock & cvar  syscall
int sys_PipeInit(UserContext *u_context){
	
	int *pipe_idp;
	pipe_info *p;
	reclaim_info *r;
	int rc;

	pipe_idp = (int*)u_context->regs[0];
	TracePrintf(2,"sys_PipeInit: Enter.\n");

	p = (pipe_info *)malloc(sizeof(pipe_info));
	r = (reclaim_info *)malloc(sizeof(reclaim_info)); 
	
	if(p==NULL){
		TracePrintf(0,"sys_PipeInit:Run out of memory\n");
		return ERROR;
	}
	
	if(r==NULL){
		TracePrintf(0,"sys_PipeInit:Run out of memory\n");
		return ERROR;
	}

	//construct the pipe
	p->pipeid = reclaim_list.len+1;
	p->cur_len = 0;
	//TODO: hardcode pipe buffer size 4KB
	p->pipe_buffer = (void*)malloc(PIPE_BUF_SIZE);
	if(p->pipe_buffer==NULL){
		TracePrintf(0,"sys_PipeInit:Run out of memory\n");
		return ERROR;
	}
	
	//initialize lock_wait_list
	p->pipe_wait_list.len=0;
	p->pipe_wait_list.head = (node*)malloc(sizeof(node));
	if(p->pipe_wait_list.head == NULL){
		TracePrintf(0,"sys_PipeInit: error: not enough mem.\n");
		return ERROR;
	}
	
  		
	*pipe_idp = p->pipeid;
	TracePrintf(2,"sys_PipeInit: pipeid %d.\n",*pipe_idp);
	rc = list_insert(&pipe_list, pipe_list.len+1,(void*)p);
	if(rc!=0){
		TracePrintf(0,"sys_PipeInit:Insert Error\n");
		return ERROR;
	}
	
	//construct the reclaim_info
	r->id = reclaim_list.len+1;
	r->type = 0; //0 for pipe
	
	rc = list_insert(&reclaim_list, reclaim_list.len+1,(void*)r);
	if(rc!=0){
		TracePrintf(0,"sys_PipeInit:Insert Error\n");
		return ERROR;
	}
	
	TracePrintf(2,"sys_PipeInit: Return\n");
  
	return 0;
	
}

int sys_PipeRead(UserContext *u_context){
	
	int pipe_id;
	void *buf, *tmp;
	int len,i,exist=0;
	pipe_info *p, *p1;
	pcb_t *next;
	node *cur,*rc;
	void* addr;
	int page_num;

	TracePrintf(2,"sys_PipeRead:Enter.\n");
	pipe_id = u_context->regs[0];
	buf = (void*)(u_context->regs[1]);
	len = u_context->regs[2];

	addr = (void*)u_context->regs[1];
	page_num = (DOWN_TO_PAGE(addr) - VMEM_1_BASE) >> PAGESHIFT;

	if(addr == NULL) {
		TracePrintf(0, "sys_Exec: error: bad argument 2\n");
		return ERROR;
	}

	if ((int)addr < VMEM_1_BASE){
		TracePrintf(0, "sys_Exec: error: cannot access kernel memory!\n");
		return ERROR;
	}

	if ( (page_num >= active_proc->next_heap_pn) && 
		(page_num <= active_proc->next_stack_pn) ) {
		TracePrintf(0, "sys_Exec: error: illegal address!\n");
		return ERROR;	 
	}

	if(pipe_id<0){
		TracePrintf(0,"sys_PipeRead:Invalid pipeid\n");
		return ERROR;
	}

	if(len<0){
		TracePrintf(0,"sys_PipeRead:Invalid len\n");
		return ERROR;
	}
	
	cur = list_index(&pipe_list, 1);
	while(cur!=NULL){
		p = (pipe_info*)(cur->data);
		if(p->pipeid==pipe_id){
			//find the correct pipe;
			while(p->cur_len<len){
				//if not enough bytes available
				list_insert(&(p->pipe_wait_list),p->pipe_wait_list.len+1,(void*)active_proc);
				if(normal_schedule(u_context)!=0){
					TracePrintf(0,"sys_PipeRead: normal_schedule error\n");
					return ERROR;
				}
				
				//wake up, check whether the pipe still exist
				for(i=1;i<=pipe_list.len;i++){
					p1 = (pipe_info*)(list_index(&pipe_list,i)->data);
					if(p1->pipeid == pipe_id){
						exist=1;
						break;
					}
				}
				
				if(exist!=1){
					TracePrintf(0,"sys_PipeRead:pipe with pipeid %d has been reclaimed\n", pipe_id);
					return ERROR;
				}
			}
			//TODO
			if(p->cur_len>=len){
				memcpy(buf,p->pipe_buffer,len);
				p->cur_len = p->cur_len-len;

				//TODO: how to free context has been read?
				//move pointer
				p->pipe_buffer = p->pipe_buffer+len;
				//using another buffer to back up
				tmp = (void*)malloc(PIPE_BUF_SIZE);
				memcpy(tmp,p->pipe_buffer,p->cur_len);
				//free current buffer
				free(p->pipe_buffer);
				//re-point buffer pointer
				p->pipe_buffer = tmp;
 
				if(p->pipe_wait_list.len>0){
					rc = list_remove(&(p->pipe_wait_list),1);
					if(rc != NULL){
						next = (pcb_t *)(rc->data);
						free(rc);
					}else{
						TracePrintf(0,"sys_PipeRead: list_remove error\n");
						return ERROR;
					}
					
					list_insert(&ready_queue, ready_queue.len+1, (void*)next);
				}
				TracePrintf(2,"sys_PipeRead:Return.\n");
				return 0;
			}	
			
		}
		cur = cur->next;
	}
	
	
	TracePrintf(0,"sys_PipeRead:pipe with pipeid %d doesn't exist\n", pipe_id);
	return ERROR;	

}

int sys_PipeWrite(UserContext *u_context){
	
	int pipe_id,buf_left,byte_write;
	void *buf;
	int len;
	pipe_info *p;
	pcb_t *next;
	node *cur, *tmp;
	int page_num;
	void* addr;

	pipe_id = u_context->regs[0];
	buf = (void*)(u_context->regs[1]);
	len = u_context->regs[2];

	addr = (void*)u_context->regs[1];
	page_num = (DOWN_TO_PAGE(addr) - VMEM_1_BASE) >> PAGESHIFT;

	if(addr == NULL) {
		TracePrintf(0, "sys_Exec: error: bad argument 2\n");
		return ERROR;
	}

	if ((int)addr < VMEM_1_BASE){
		TracePrintf(0, "sys_Exec: error: cannot access kernel memory!\n");
		return ERROR;
	}

	if ( (page_num >= active_proc->next_heap_pn) && 
		(page_num <= active_proc->next_stack_pn) ) {
		TracePrintf(0, "sys_Exec: error: illegal address!\n");
		return ERROR;	 
	}

	TracePrintf(2,"sys_PipeWrite:Enter\n");
	TracePrintf(2,"sys_PipeWrite:Buffer Context %s\n",(char*)buf);

	if(pipe_id<0){
		TracePrintf(0,"sys_PipeWrite:Invalid pipeid\n");
		return ERROR;
	}

	if(len<0){
		TracePrintf(0,"sys_PipeWrite:Invalid len\n");
		return ERROR;
	}	

	cur = list_index(&pipe_list,1);
	while(cur!=NULL){
		p = (pipe_info*)(cur->data);
		if(p->pipeid == pipe_id){
			//find the correct pipe
			buf_left = PIPE_BUF_SIZE-p->cur_len;
			
			TracePrintf(2,"sys_PipeWrite: cur->len %d, buf_left %d len %d.\n",p->cur_len, buf_left,len);
			if(buf_left<len){
				//not enough space to write
				memcpy(p->pipe_buffer + p->cur_len,buf,buf_left);
				//TracePrintf(2,"sys_PipeWrite: buffer %s.\n",(char*)buf);
				//TracePrintf(2,"sys_PipeWrite: buffer content %s.\n",(char*)p->pipe_buffer);
				byte_write = buf_left;
				TracePrintf(0,"sys_PipeWrite: Pipe buf exceeds, only %d bytes written\n",byte_write);
			}
			else{
				//enough space to write
				memcpy(p->pipe_buffer + p->cur_len,buf,len);
				//TracePrintf(2,"sys_PipeWrite: buffer %s.\n",(char*)buf);
				//TracePrintf(2,"sys_PipeWrite: buffer content %s.\n",(char*)p->pipe_buffer);
				byte_write = len;
				TracePrintf(2,"sys_PipeWrite: %d bytes written\n",byte_write);
			}
			p->cur_len = p->cur_len + byte_write;

			if(p->pipe_wait_list.len>0){
				tmp = list_remove(&(p->pipe_wait_list),1);
				if(tmp!=NULL){
					next = (pcb_t*)(tmp->data);
					free(tmp);
				}else{
					TracePrintf(0,"sys_PipeWrite: list_remove ERROR\n");
					return ERROR;
				}
				list_insert(&ready_queue, ready_queue.len+1,(void*)next);
			}
			TracePrintf(2,"sys_PipeWrite:Return with %d bytes writen\n",byte_write);
			TracePrintf(2,"sys_PipeWrite: buffer content %s.\n",(char*)p->pipe_buffer);
			return byte_write;
		}
		cur = cur->next;
	}
		
	TracePrintf(0,"sys_PipeWrite:pipe with pipeid %d doesn't exist\n", pipe_id);
	return ERROR;
	
}


int sys_LockInit(UserContext *u_context){
	//create a new lock
	int *lock_idp;
	lock_info *l;
	reclaim_info *r;
	int rc;
	
	lock_idp = (int*)u_context->regs[0];
	TracePrintf(2,"sys_LockInit: Enter.\n");

	l = (lock_info *)malloc(sizeof(lock_info));
	r = (reclaim_info *)malloc(sizeof(reclaim_info));

	if(l==NULL){
		TracePrintf(0,"sys_LockInit:Run out of memory\n");
		return ERROR;
	}
	
	if(r==NULL){
		TracePrintf(0,"sys_LockInit:Run out of memory\n");
		return ERROR;
	}

  	//construct the lock_info
	l->lockid = reclaim_list.len+1;
	l->locked = 0;
	l->pid = NULL;

	//initialize lock_wait_list
	l->lock_wait_list.len=0;
	l->lock_wait_list.head = (node*)malloc(sizeof(node));
	if(l->lock_wait_list.head == NULL){
		TracePrintf(0,"sys_LockInit: error: not enough mem.\n");
		return ERROR;
	}
	
  		
	*lock_idp = l->lockid;
	TracePrintf(2,"sys_LockInit: lockid %d.\n",*lock_idp);
	rc = list_insert(&lock_list, lock_list.len+1,(void*)l);
	if(rc!=0){
		TracePrintf(0,"sys_LockInit:Insert Error\n");
		return ERROR;
	}
	
	//construct the reclaim_info
	r->id = reclaim_list.len+1;
	r->type = 1; //1 for lock
	
	rc = list_insert(&reclaim_list, reclaim_list.len+1,(void*)r);
	if(rc!=0){
		TracePrintf(0,"sys_LockInit:Insert Error\n");
		return ERROR;
	}
	
	TracePrintf(2,"sys_LockInit: Return\n");
  
	return 0;
}

int sys_Acquire(UserContext *u_context, int id){
	int lock_id,i,exist=0;
	lock_info *l,*l1;
	node *cur;

	
	if(id==NULL) lock_id = u_context->regs[0];
	else lock_id=id;
	
	if(id<0){
		TracePrintf(0,"sys_Acquire: error, lock id <0\n");
		return 0;
	}
	TracePrintf(2,"sys_Acquire: Enter lockid %d.\n",lock_id);

	//check whether this lock is available
	cur = list_index(&lock_list,1);
	while(cur!=NULL){
		l = (lock_info*)(cur->data);
		//if lock with this lock_id exist
		if(l->lockid == lock_id){

			//if locked, insert into waiting list
			while(l->locked ==1){
				TracePrintf(2,"sys_Acquire:lock %d is being used.\n", lock_id);
				list_insert(&(l->lock_wait_list),(l->lock_wait_list).len+1, (void*)active_proc);

				if(normal_schedule(u_context)!=0){
					TracePrintf(0,"sys_Acquire: normal_schedule error\n");
					return ERROR;
				}

				//wake up check lock existence
				for(i=1;i<=lock_list.len;i++){
					l1 = (lock_info*)(list_index(&lock_list,i)->data);
					if(l1->lockid == lock_id){
						exist = 1;
						break;
					}
				}
				if(exist!=1){
					TracePrintf(0,"sys_Acquire:error,lock %d has been reclaimed.\n", lock_id);
					return ERROR;
				}
			}
      			
			//if not locked update
			if(l->locked == 0){
				l->locked = 1;
				l->pid = active_proc->pid;				
				TracePrintf(2,"sys_Acquire: Return.\n");				
				return 0;
			}
    
    		}
		cur = cur->next;
  	}

	//if this lock hasn't been created
	TracePrintf(0,"sys_Acquire:error,lock %d doesn't exist.\n", lock_id);
	return ERROR;
}

int sys_Release(int lock_id){
	//int lock_id = u_context->regs[0];
	lock_info *l;
	pcb_t *p;
	node *cur,*rc;
  	
	TracePrintf(2,"sys_Release: Enter.\n");
	cur = list_index(&lock_list,1);
	while(cur!=NULL){
		l = (lock_info*)(cur->data);

		//find the correct lock_info
		if(l->lockid==lock_id &&
		   l->pid==active_proc->pid &&
		   l->locked==1){

			l->locked = 0;
			l->pid = NULL;
			
			//if any process waiting for lock
			if(l->lock_wait_list.len>0){
				//remove first node from lock waiting list
				rc = list_remove(&(l->lock_wait_list),1);
				if(rc!=NULL){
					p = (pcb_t*)(rc->data);
					free(rc);
				}else{
					TracePrintf(0,"sys_Release:list_remove Error\n");
					return ERROR;
				}
				l->pid = p->pid;
				//insert into ready queue
				list_insert(&ready_queue,ready_queue.len+1,(void*)p);
			}
			TracePrintf(2,"sys_Release: Return.\n");	
			return 0;
			
		}
		cur = cur->next;
	}

	//no lock find
	TracePrintf(0,"sys_Release: lock %d doesn't exist.\n", lock_id);
	return ERROR;
}

//cvar
int sys_CvarInit(UserContext* u_context){
	//create a nre cvar
	int *cvar_idp = (int*)u_context->regs[0];
	reclaim_info *r;
	cvar_info *c;
	node *cur;
	int rc;

	TracePrintf(2,"sys_CvarInit: Enter.\n");
	r = (reclaim_info *)malloc(sizeof(reclaim_info));
	c = (cvar_info *)malloc(sizeof(cvar_info));

	if(c==NULL){
		TracePrintf(0,"sys_CvarInit:Run out of memory\n");
		return ERROR;
	}
  	//construct cvar_info
	c->cvarid = reclaim_list.len+1;
	//initialize cvar_wait_list
	c->cvar_wait_list.len=0;
	c->cvar_wait_list.head = (node*)malloc(sizeof(node));
	if(c->cvar_wait_list.head == NULL){
		TracePrintf(0,"sys_CvarInit: error: not enough mem.\n");
		return ERROR;
	}
  
	*cvar_idp = c->cvarid;

	rc = list_insert(&cvar_list, cvar_list.len+1,(void*)c);
	if(rc!=0){
		TracePrintf(0,"sys_CvarInit:Insert Error\n");
		return ERROR;
	}

  	//construct reclaim_info
	r->id = reclaim_list.len+1;
	r->type = 2;//2 for cvar
	
	rc = list_insert(&reclaim_list,reclaim_list.len+1,(void*)r);
	if(rc!=0){
		TracePrintf(0,"sys_CvarInit:Insert Error\n");
		return ERROR;
	}
		
	TracePrintf(2,"sys_CvarInit: Return\n");
  
	return 0;
}

int sys_CvarSignal(UserContext *u_context){
	int cvar_id;
	cvar_info *c;
	pcb_t *p;
	node *cur, *tmp;
	int rc;

	cvar_id = u_context->regs[0];
	TracePrintf(2,"sys_CvarSignal: Enter. cvar_id %d\n",cvar_id);
	if(cvar_id<0){ 
		TracePrintf(0,"sys_CvarSignal:Invalid cvar_id\n");
		return ERROR;
	}
	cur = list_index(&cvar_list,1);

	while(cur!=NULL){
		//TracePrintf(2,"sys_CvarSignal: 1\n");
		c = (cvar_info*)(cur->data);
		TracePrintf(2,"sys_CvarSignal: c->cvarid %d\n",c->cvarid);
		if(c->cvarid==cvar_id){
			
			if(c->cvar_wait_list.len<0){
				TracePrintf(0,"sys_CvarSignal:cvar_wait_list uninitialized\n");
				return ERROR;
			}
			
			if(c->cvar_wait_list.len==0){ 
				TracePrintf(2,"sys_CvarSignal: No object to signal.\n");
				return 0;
			}
			
			if(c->cvar_wait_list.len>0){
				//activate first element in the queue
				tmp = list_remove(&(c->cvar_wait_list),1);
				if(tmp!=NULL){
					p = (pcb_t*)(tmp->data);
					free(tmp);
				}else{				
					TracePrintf(0,"sys_CvarSignal:list_remove Error\n");
					return ERROR;
				}

				rc = list_insert(&ready_queue,ready_queue.len+1,(void*)p);
				if(rc!=0){
						TracePrintf(0,"sys_CvarSignal:Insert Error\n");
						return ERROR;
				}
				TracePrintf(2,"sys_CvarSignal: Return.\n");
				return 0;
			}


		}
		cur = cur->next;
	}

	//no cvar found
	TracePrintf(0,"sys_CvarSignal: cvar with cvar_id %d doesn't exist\n",cvar_id);
	return ERROR;
}

int sys_CvarBroadcast(UserContext *u_context){
	int cvar_id = u_context->regs[0];
	cvar_info *c;
	pcb_t* p;
	node *cur,*tmp;
	int rc;
	if(cvar_id<0){
		TracePrintf(0,"sys_CvarBroadcast:Invalid cvar_id\n");
		return ERROR;
	}

	TracePrintf(2,"sys_CvarBroadcast: Enter.\n");
	cur = list_index(&cvar_list,1);

	while(cur!=NULL){
		c = (cvar_info*)(cur->data);

		if(c->cvarid==cvar_id){

			if(c->cvar_wait_list.len<0){
				TracePrintf(0,"sys_CvarBroadcast:cvar_wait_list uninitialized\n");
				return ERROR;
			}
			if(c->cvar_wait_list.len==0){
				TracePrintf(2,"sys_CvarBroadcast: No one to broadcast.\n"); 
				return 0;
			}
			while(c->cvar_wait_list.len>0){
				TracePrintf(2,"sys_CvarBroadcast: len %d.\n",c->cvar_wait_list.len);
				//active all elements in the cvar_wait_list
				tmp = list_remove(&(c->cvar_wait_list),1);
				if(tmp!=NULL){
					p = (pcb_t*)(tmp->data);
					free(tmp);
				}else{
					TracePrintf(0,"sys_CvarBroadcast:list_remove Error\n");
				 	return ERROR;

				}

				rc = list_insert(&ready_queue,ready_queue.len+1,(void*)p);
				if(rc!=0){
					TracePrintf(0,"sys_CvarBroadcast:Insert Error\n");
					return ERROR;
				}

			}	
			TracePrintf(2,"sys_CvarBroadcast: Return.\n");
			return 0;		

		}
		cur = cur->next;
	}

	//no cvar found
	TracePrintf(0,"sys_CvarBroadcast: cvar with cvarid %d doesnt exist.\n",cvar_id);
	return ERROR; 

}


int sys_CvarWait(UserContext* u_context){
	int cvar_id = u_context->regs[0];
	int lock_id = u_context->regs[1];
	lock_info *l;
	cvar_info *c, *c1;
	pcb_t* p, next;
	node *cur;
	int rc,i,exist=0;
	
	TracePrintf(2,"sys_CvarWait: Enter.\n");
	
	//release lock
	TracePrintf(2,"sys_CvarWait: Release lock %d.\n",lock_id);
	rc = sys_Release(lock_id);
	if(rc!=0) {
		TracePrintf(0,"sys_CvarWait:Release Error\n");
		return ERROR;
	}

	//wait for cvar in the wait list
	cur = list_index(&cvar_list,1);
	while(cur!=NULL){
		c = (cvar_info*)(cur->data);

		if(c->cvarid==cvar_id){
			list_insert(&(c->cvar_wait_list),c->cvar_wait_list.len+1,(void*)active_proc);
			TracePrintf(2,"sys_CvarWait: len %d.\n",c->cvar_wait_list.len);
			
			if(normal_schedule(u_context)!=0){
				TracePrintf(0,"sys_CvarWait: normal_schedule error\n");
				return ERROR;
			}
				
			//wake up, check the existence of cvar
			for(i=1;i<=cvar_list.len;i++){
				c1 = (cvar_info*)(list_index(&cvar_list,i)->data);
				if(c1->cvarid==cvar_id){
					exist = 1;
					break;
				}
				
			}
			
			if(exist!=1){
				TracePrintf(0,"sys_CvarWait: cvar with cvarid %d has reclaimed.\n",cvar_id);
				return ERROR;
			}
			
			//try to acquire lock
			TracePrintf(2,"sys_CvarWait: Acquire lock.\n", lock_id);
			rc = sys_Acquire(u_context,lock_id);
			if(rc!=0){
				TracePrintf(0,"sys_CvarWait:Acquire Error\n");
				return ERROR;
			}
			TracePrintf(2,"sys_CvarWait: Return.\n");
			return 0;
		}
		cur =cur->next;
	}

	//find no cvar 
	TracePrintf(0,"sys_CvarWait: cvar with cvarid %d doesnt exist.\n",cvar_id);
	return ERROR;

}


int sys_Reclaim(UserContext* u_context){
	int id,i,rc;	
	reclaim_info *r;
	node *cur;
	
	TracePrintf(2,"sys_Reclaim: enter.\n");
	id = u_context->regs[0];
	if(id<0) {
		TracePrintf(0,"sys_Reclaim: Invalid id.\n");
		return ERROR;
	}	
	
	//find the right list out of 3
	cur = list_index(&reclaim_list,1);
	i=1;
	while(cur!=NULL){
		r = (reclaim_info*)(cur->data);
		
		if(id == r->id){
			switch (r->type){
				case 0: 
					rc =  destroy_pipe_info(id);//sche
					if(rc!=0){
						TracePrintf(0,"sys_Reclaim: Error destroy_pipe_info.\n");
						return ERROR;
					}					
					break;
				case 1: 
					rc = destroy_lock_info(id);
					if(rc!=0){
						TracePrintf(0,"sys_Reclaim: Error destroy_lock_info.\n");
						return ERROR;
					}
					break; 
				case 2: 
					rc = destroy_cvar_info(id);
					if(rc!=0){
						TracePrintf(0,"sys_Reclaim: Error destroy_cvar_infor.\n");
						return ERROR;
					}
					break;

				default: TracePrintf(0,"sys_Reclaim: Error undefined type\n");
					return ERROR;
			}
				
			
			//free reclaim_info;
			list_remove(&reclaim_list,i);
			free(r);
			TracePrintf(2,"sys_Reclaim: return\n");
			return 0;
		}
	i++;
	cur = cur->next;
		
	}
		
	//find no match with id
	TracePrintf(0,"sys_Reclaim: no match for id %d\n",id);
	return ERROR;

}

int normal_schedule(UserContext* u_context){
	node *rc;
	pcb_t *next;

	if(ready_queue.len>0){
		rc = list_remove(&ready_queue, 1);
		if(rc!=NULL){
			next = (pcb_t*)(rc->data);
			free(rc);
		}else{
			TracePrintf(0,"normal_schedule: list_remove error\n");	
			return ERROR;
		}
	

		// check whether next is idle
	    	if(next->pid == 1){
	      		//insert it to the end of ready_queue and re-do the context switch			
	      		list_insert(&ready_queue, ready_queue.len+1, (void*)next);
			rc = list_remove(&ready_queue,1);
			if(rc!=NULL){
				next = (pcb_t *)(rc->data);
				free(rc);
			}else{
				TracePrintf(0,"normal_schedule: list_remove error\n");	
				return ERROR;
			}
	      		
	    	}
		context_switch(u_context, active_proc, next);
	}
	TracePrintf(2, "normal_scheduler: returning...\n");
	return 0;
}

// work as sem_init
int sys_Custom_0(UserContext* u_context){
	semaphore* tmp = (semaphore*)malloc(sizeof(semaphore));
	if (tmp == NULL){
		TracePrintf(0, "sys_Custom_0: error: not enough kernel mem\n");
		return ERROR;
	}

	tmp->id = next_sem++;
	tmp->count = 0;

	(tmp->waiting).len = 0;
	(tmp->waiting).head = (node*)malloc(sizeof(node));
	if((tmp->waiting).head == NULL){
	  	TracePrintf(0, "sys_Custom_0: error: not enough kernel mem.\n");
		return ERROR;
	}

	list_insert(&sem_list, sem_list.len+1, (void*)tmp);
	return next_sem - 1;
}

// work as sem_inc
int sys_Custom_1(UserContext* u_context){
	int id = u_context->regs[0];
	int i = 1;
	node* nod = (node*)list_index(&sem_list, i);
	semaphore* sem = (semaphore*)(nod->data);
	while(sem != NULL){
		if (sem->id == id){	// found the sem
			TracePrintf(3, "sys_Custom_1: found the sem\n");
			break;
		}
		else{
			i++;
			nod = (node*)list_index(&sem_list, i);
			sem = (semaphore*)(nod->data);
		}
	}

	if (sem == NULL){	// sem not exist
		TracePrintf(0, "sys_Custom_1: error: cannot increment non-existing sem\n");
		return ERROR;
	}

	// inc the sem
	sem->count ++;
	if (sem->count <= 0){	// if someone waiting, kick him up
		node* temp;
		pcb_t* wake_up;
		if ((sem->waiting).len > 0){	// someone waiting
			TracePrintf(3, "sys_Custom_1: kicking someone up\n");
			temp = (node*)list_remove(&(sem->waiting), 1);
			wake_up = (pcb_t*)(temp->data);
			free(temp);		
			list_insert(&ready_queue, ready_queue.len+1, (void*)(wake_up));
			TracePrintf(3, "sys_Custom_1: process %d is ready now!\n", wake_up->pid);
		}		
	}	
	// mark: return 0 upon success
	return 0;
}

// work as sem_wait
int sys_Custom_2(UserContext* u_context){
	int id = u_context->regs[0];
	int i = 1;
	node* nod = (node*)list_index(&sem_list, i);
	semaphore* sem = (semaphore*)(nod->data);

	node* nod2 = (node*)list_index(&sem_list, i+1);
//	TracePrintf(0, "sys_Custom_2: argument id: %d\n", id);
//	TracePrintf(0, "sys_Custom_2: sem_list len: %d\n", sem_list.len);
//	TracePrintf(0, "sys_Custom_2: first sem in list: %d\n", ((semaphore*)(nod->data))->id);
//	TracePrintf(0, "sys_Custom_2: second sem in list: %d\n", ((semaphore*)(nod2->data))->id);

	while(sem != NULL){
		//TracePrintf(0, "sys_Custom_2: sem->id: %d, id: %d\n", sem->id, id);
		if ( (sem->id) == id){	// found the sem
			TracePrintf(3, "sys_Custom_2: found the sem\n");
			break;
		}
		else{
			i++;
			nod = (node*)list_index(&sem_list, i);
			sem = (semaphore*)(nod->data);
		}
	}

	if (sem == NULL){	// sem not exist
		TracePrintf(0, "sys_Custom_2: error: cannot wait on non-existing sem\n");
		return ERROR;
	}

	// dec the sem
	sem->count --;
	if (sem->count < 0){	// oops, have to block you...
		list_insert(&(sem->waiting), (sem->waiting).len+1, (void*)active_proc);	
		if ( normal_schedule(u_context) == ERROR) {
			TracePrintf(0, "sys_Custom_2: error context switching\n");
			return ERROR;
		}
	}
	// mark: return 0 upon success
	return 0;
}
