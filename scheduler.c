#include "scheduler.h"
#include "kernel.h"

KernelContext* switch_kernel_context(	KernelContext* kcs_in,	
					void* p_old,
					void* p_new
					)
{
	int i;
	void *dst, *src;
	pcb_t *old, *new;
	old = p_old; new = p_new;

	TracePrintf(2, "switch_kernel_context: entered.\n");
	
	if(p_old!=NULL){
		// save old kernel context
		memcpy(&(old->kctxt), kcs_in, sizeof(KernelContext));
	
		// save kernel stack
		TracePrintf(3, "switch_kernel_context: saving %d's kernel stack...\n", old->pid);
		TracePrintf(3, "switch_kernel_context: old stack pfn 0: %d\n", old->k_stack_pt[0].pfn);
		TracePrintf(3, "switch_kernel_context: old stack pfn 1: %d\n", old->k_stack_pt[1].pfn);
		for (i = 0; i < 2; i ++){
			src = (void*) ((krnl_page_table[126+i].pfn) << PAGESHIFT);
			dst = (void*) ((old->k_stack_pt[i].pfn) << PAGESHIFT);
			TracePrintf(3, "switch_kernel_context: src = %p, dst = %p, saving...\n", src, dst);
			memcpy(dst, src, PAGESIZE);
		}
		
	}
	

	if(p_new!=NULL){
		// restore new kernel stack
		TracePrintf(3, "switch_kernel_context: restoring %d's kernel stack...\n", new->pid);
		TracePrintf(3, "switch_kernel_context: new stack pfn 0: %d\n", new->k_stack_pt[0].pfn);
		TracePrintf(3, "switch_kernel_context: new stack pfn 1: %d\n", new->k_stack_pt[1].pfn);
		for (i = 0; i < 2; i ++){
	    		src = (void*) ((new->k_stack_pt[i].pfn) << PAGESHIFT);
	    		dst = (void*) ((krnl_page_table[126+i].pfn) << PAGESHIFT);	
	    		TracePrintf(3, "switch_kernel_context: src = %p, dst = %p, recovering...\n", src, dst);
	    		memcpy(dst, src, PAGESIZE);
		}
	
	
		WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
		//list_insert(&ready_queue, ready_queue.len+1, (void*)old);
	
		// return new kernel context
		TracePrintf(3, "switch_kernel_context: restoring new kernel context....\n");
		memcpy(kcs_in, &(new->kctxt),sizeof(KernelContext));
	}
	TracePrintf(2, "switch_kernel_context: returning...\n");
	return kcs_in;
}

void context_switch(UserContext* uctxt, pcb_t* old, pcb_t* new)
{
	int rc;

	TracePrintf(2, "context_switch: entered.\n");

	active_proc = new;
	
	if(old != NULL){
	//mark: can't distiguish ERROR or real NULL
	  TracePrintf(3, "context_switch: saving process %d's old uctxt...\n", old->pid);
	  memcpy(&(old->uctxt), uctxt, sizeof(UserContext));
	}
	// check if kernel stack & context initialized
	if (new->k_stack_pt[0].pfn == 0) {
		TracePrintf(3, "context_switch: initializing kernel context for process %d\n", new->pid);	
		KernelContextSwitch(init_proc_kernel, new, NULL);
	}
	// switch kernel context
	  
	rc = KernelContextSwitch(switch_kernel_context, old, new);	
	if (rc != 0){
		TracePrintf(0, "context_switch: error in KernelContextSwitch.\n");
	}
	else {
		TracePrintf(3, "context_switch: returned form kcs. process %d is back\n", old->pid);
	}
	

	// modify user pagetable
	WriteRegister(REG_PTBR1, (unsigned int)(old->u_page_table));
	WriteRegister(REG_PTLR1, (unsigned int)(USER_PAGETABLE_SIZE));
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	 
	// restore old user context
	
	memcpy(uctxt, &(old->uctxt), sizeof(UserContext));
	
	TracePrintf(3, "context_switch: cur pid: %d\n", old->pid);
	TracePrintf(3, "context_switch: cur pc: %p, cur sp: %p\n", uctxt->pc, uctxt->sp);

	TracePrintf(2, "context_switch: returning...\n");
}

KernelContext* save_initial_kernel(	KernelContext* kcs_in,	
					void* not_used,
					void* useless
					)
{
	int i;
	int pfn;
	void *dst, *src;

	TracePrintf(2, "save_initial_kernel: entered\n");
	memcpy(&initial_kc, kcs_in, sizeof(KernelContext));
	for (i = 0; i < 2; i++){
		initial_ks[i].valid = 1;
		initial_ks[i].prot = PROT_READ | PROT_WRITE;
		if (0 == (pfn = grab_a_frame())){
			TracePrintf(0, "save_initial_kernel: error: not enough mem\n");
			return NULL;
		}
		initial_ks[i].pfn = pfn;
		dst = (void*)(initial_ks[i].pfn << PAGESHIFT);
		src = (void*)(krnl_page_table[126+i].pfn << PAGESHIFT);
		memcpy(dst, src, PAGESIZE);
	}
	TracePrintf(2, "save_initial_kernel: returning...\n");
	return &initial_kc;
}

KernelContext* init_proc_kernel	(	KernelContext* kcs_in,	
					void* p_pcb,
					void* not_used
					)
{
	int i;
	void *src, *dst;
	int pfn;
	pcb_t* cur = (pcb_t*)p_pcb;

	TracePrintf(2, "init_proc_kernel: entered\n");

	memcpy(&(cur->kctxt), &initial_kc, sizeof(KernelContext));


	for (i = 0; i < 2; i ++){
		cur->k_stack_pt[i].valid = 1;
		cur->k_stack_pt[i].prot = PROT_READ | PROT_WRITE;
		if (0 == (pfn = grab_a_frame())){
			TracePrintf(0, "init_proc_kernel: error grabbing a frame.\n");
			return;
		}
		cur->k_stack_pt[i].pfn = pfn;
		dst = (void*) ((cur->k_stack_pt[i].pfn) << PAGESHIFT);
		src = (void*) ((initial_ks[i].pfn) << PAGESHIFT);
		memcpy(dst, src, PAGESIZE);
	}
	TracePrintf(2, "init_proc_kernel: returning...\n");
	return kcs_in;	
}

/* return kernel to initial state */ 
KernelContext* return_initial_state(	KernelContext* kcs_in,	
					void* not_used,
					void* useless
					)
{
	int i;
	void *src, *dst;

	TracePrintf(2, "return_initial_state: entered\n");

	for (i = 0; i < 2; i ++){
		dst = (void*) ((krnl_page_table[126+i].pfn) << PAGESHIFT);
		src = (void*) ((initial_ks[i].pfn) << PAGESHIFT);
		TracePrintf(3, "return_initial_state: src: %p, dst: %p\n", src, dst);
		memcpy(dst, src, PAGESIZE);
	}
	memcpy(kcs_in, &initial_kc, sizeof(KernelContext));
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
	TracePrintf(2, "return_initial_state: returning...\n");
	return kcs_in;
}
/*
insert_sleep_list() constructs a new sleep_info and 
insert it into sleep_list
*/
int insert_sleep_list(pcb_t *p, int clock_ticks)
{
	sleep_info *s;

	TracePrintf(2, "insert_sleep_list: entered.\n");	
	s = (sleep_info *)malloc(sizeof(sleep_info));
	
	if(s == NULL){
		TracePrintf(0, "insert_sleep_list: Run out of memory\n");
		return ERROR;
	}

	p->state = DELAY;
	s->pcb = p;
	s->ticks_left = clock_ticks;
	
	if(list_insert(&sleep_list,sleep_list.len+1,(void*)s) != 0){	  
		//TracePrintf(0, "insert_sleep_list: list_sert error\n");
		free(s);
		return ERROR;
	}
	
	TracePrintf(2, "insert_sleep_list: returning...\n");
	return 0;
}

/*
insert_dead_list() constructs a new dead_info and 
insert it into dead_list
*/
int insert_dead_list(int ppid, int pid, int status)
{
	dead_info *d;
	
	TracePrintf(2, "insert_dead_list: entered.\n");	
	d = (dead_info *)malloc(sizeof(dead_info));
	
	if(d == NULL){
		TracePrintf(0, "insert_dead_list: list_sert error\n");
		return ERROR;
	}
	d->ppid = ppid;
	d->pid = pid;
	d->status = status;
	
	if(list_insert(&dead_list,dead_list.len+1,(void*)d) != 0){	  
		TracePrintf(0, "insert_dead_list: list insert error\n");
		free(d);
		return ERROR;
	}
	
	TracePrintf(2, "insert_dead_list: returning...\n");
	return 0;

}
/*
destroy one pipe_info with pipe_id
*/
int destroy_pipe_info(int pipe_id){
	int i,j;	
	node *cur, *tmp;
	pipe_info *pi;
	pcb_t *p;

	TracePrintf(2,"destroy_pipe_info: enter.\n");
	if(pipe_id<0){
		TracePrintf(0,"destroy_pipe_info: error, pipe_id<0\n");
		return ERROR;
	} 

	i=1;
	cur  = list_index(&pipe_list,1);
	while(cur!=NULL){
		pi = (pipe_info*)(cur->data);
		if(pi->pipeid == pipe_id){
			//find the correct pipe
			//free buffer
			free(pi->pipe_buffer);
			//handle all pcb in wait_list
			if(pi->pipe_wait_list.len>0){
				for(j=1;j<= pi->pipe_wait_list.len;j++){
					tmp = list_remove(&(pi->pipe_wait_list),1);
					if(tmp!=NULL){
						p = (pcb_t*)(tmp->data);
						free(tmp);
					}else{
						TracePrintf(0,"destroy_pipe_info: list remove error\n");
						return ERROR;
					}
					//TODO:test illegal trap
					list_insert(&ready_queue, ready_queue.len+1,(void*)p);
					TracePrintf(2,"move from pipe_wait_list to ready_queue: pid %d\n",p->pid);
				}

			}
			//free pipe_info
			free(pi);
			//remove it from pipe_list
			tmp = list_remove(&pipe_list,i);
			if(tmp==NULL){
				TracePrintf(0,"destroy_pipe_info: list remove error\n");
				return ERROR;
			}else{
				free(tmp);
			}
			TracePrintf(2,"destroy_pipe_info: return.\n");
			return 0;
	
		}
		i++;
		cur = cur->next;
	}
	
	//find no pipe
	TracePrintf(0, "destroy_pipe_info: error, no match find for pipe_id %d.\n", pipe_id);
	return ERROR;
}

/*
destroy one lock_info with lock_id
*/
int destroy_lock_info(int lock_id)
{
	int i,j;
	node *cur, *tmp;
	lock_info *l;
	pcb_t *p;
	
	TracePrintf(2, "destroy_lock_info: enter.\n");
	if(lock_id<0){
		TracePrintf(0,"destroy_lock_info: error, lock_id<0\n");
		return ERROR;
	} 

	i=1;
	cur = list_index(&lock_list,1);
	while(cur!=NULL){
		l = (lock_info*)(cur->data);
		if(l->lockid == lock_id && ((l->pid == active_proc->pid && l->locked == 1)||(l->locked == 0))){
			
			if(l->lock_wait_list.len>0){
				// Unblock all waiting processes and have them return ERROR
				for(j=1;j<=l->lock_wait_list.len;j++){
					tmp = list_remove(&(l->lock_wait_list),1);
					if(tmp!=NULL){
						p = (pcb_t*)(tmp->data);
						free(tmp);
					}else{
						TracePrintf(0,"destroy_lock_info: list remove error\n");
						return ERROR;	
					}
					
					list_insert(&ready_queue, ready_queue.len+1,(void*)p);
					TracePrintf(2,"move from lock_wait_list to ready_queue: pid %d\n",p->pid);
				}
			}
			//free lock_info
			free(l);
			//remove it from lock_list
			tmp = list_remove(&lock_list,i);
			if(tmp ==NULL){
				TracePrintf(0,"destroy_lock_info: list remove error\n");
				return ERROR;
			}else{
				free(tmp);
			}
	
			TracePrintf(2, "destroy_lock_info: return.\n");
			return 0;	
		}
		i++;
		cur = cur->next;
	}	
	
	//find no lock
	TracePrintf(0, "destroy_lock_info: error, no match find for lock_id %d.\n", lock_id);
	return ERROR;

}
/*
destroy one cvar_info with cvar_id
*/
int destroy_cvar_info(int cvar_id){
	
	int i,j;
	node *cur, *tmp;
	cvar_info *c;
	pcb_t *p;
	
	TracePrintf(2, "destroy_cvar_info: enter.\n");
	if(cvar_id<0){
		TracePrintf(0,"destroy_cvar_info: error, lock_id<0\n");
		return ERROR;
	}
	cur = list_index(&cvar_list,1);
	i=1;
	while(cur!=NULL){
		c = (cvar_info*)(cur->data);
		if(c->cvarid==cvar_id){
			if(c->cvar_wait_list.len>0){
				// Unblock all waiting processes and have them return ERROR
				for(j=1;j<=c->cvar_wait_list.len;j++){
					tmp = list_remove(&(c->cvar_wait_list),1);
					if(tmp!=NULL){
						p = (pcb_t*)(tmp->data);
						free(tmp);
					}else{
						TracePrintf(0,"destroy_cvar_info: list_remove error\n");
						return ERROR;
					}
					
					list_insert(&ready_queue, ready_queue.len+1,(void*)p);
					TracePrintf(2,"move from cvar_wait_list to ready_queue: pid %d\n",p->pid);
				}
			}

			free(c);
			tmp = list_remove(&cvar_list,i);
			if(tmp ==NULL){
				TracePrintf(0,"destroy_cvar_info: list remove error\n");
				return ERROR;
			}else{
				free(tmp);
			}

			TracePrintf(2, "destroy_cvar_info: return.\n");
			return 0;
		}
		
		i++;
		cur = cur->next; 
	}

	//find no cvar
	TracePrintf(0, "destroy_cvar_info: error, no match find for cvar_id %d.\n", cvar_id);
	return ERROR;

}
/*
update every delay_info in delay_list every clock cycle
*/
int update_sleep_list()
{
	node *curr,*rc;
    	sleep_info *s;
	int i;

	i = 1;
	curr = list_index(&sleep_list,1);
	
	TracePrintf(2, "update_sleep_list: entered\n");

    	while ( curr != NULL ) {
		s = (sleep_info*)(curr->data);
		s->ticks_left--;
		TracePrintf(3, "update_sleep_list: p %d ticks_left %d\n",i,s->ticks_left);
		
		if (s->ticks_left <= 0) {		
			// put on the ready queue
			TracePrintf(3, "update_sleep_list: recover process\n");
			s->pcb->state = READY;
			list_insert(&ready_queue, ready_queue.len+1, (void*)s->pcb);
			TracePrintf(3, "update_sleep_list: remove process\n");
            		rc = list_remove(&sleep_list, i);
			if(rc!=NULL){
				i--;
				free(rc);
			}else{			
				TracePrintf(0, "update_sleep_list: remove error\n");
				return ERROR;
			}
			TracePrintf(3, "update_sleep_list: sleep len %d\n",sleep_list.len);
			free(s);
        	} 
			
		i++;
		curr = curr->next;
					       
    	}

	TracePrintf(2, "update_sleep_list: returning...\n");
	return 0;
}


/*
schedule processes in ready queue using round-robin alogrithm every clock cycle 
*/
int do_scheduler(UserContext *u_context)
{
	node *tmp;
	pcb_t *next;

	TracePrintf(2, "do_scheduler: entered.\n");
	/* If we have another thread waiting on the ready queue,
	we'll switch contexts. */
	TracePrintf(3, "do_scheduler: ready_queue len: %d\n", ready_queue.len);
	if (ready_queue.len > 0 ) {
		TracePrintf(3, "do_scheduler: someone is ready. switching to it...\n");
	    	pcb_t* old = active_proc;
		// mark: 
		list_insert(&ready_queue, ready_queue.len+1, (void*)old);
		tmp = list_remove(&ready_queue, 1);
		if(tmp!=NULL){
	    		next = (pcb_t*)(tmp->data);
			free(tmp);
		}else{
			TracePrintf(0,"do_scheduler: list_remove error\n");
			return ERROR;
		}
	    	TracePrintf(3, "do_scheduler: remove\n");
    		// check whether next is idle
	    	if(next->pid == 1){
			//if it is, insert it to the end of ready_queue 
			//and re-grasp the first element in ready queue to end
	      		TracePrintf(3,"next schedule pcb is idle\n");
	      		list_insert(&ready_queue, ready_queue.len+1, (void*)next);
			
			tmp = list_remove(&ready_queue,1);
			if(tmp!=NULL){
	      			next = (pcb_t *)(tmp->data);
				free(tmp);
			}else{
				TracePrintf(0,"do_scheduler: list_remove error\n");
				return ERROR;
			}
	    	}
	    	/* Switch contexts. */
	    	// mark: I think we shouldn't do this
	    	//old->state = READY;
		
	    	//next->state = RUNNING;		
	    	context_switch(u_context,old,next);
  	}
  	TracePrintf(2, "do_scheduler: returning...\n");
	return 0;
}


