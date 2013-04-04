#include "trap.h"
#include "kernel.h"	
#include "hardware.h"	
#include "yalnix.h"	
#include "scheduler.h"

void trap_undefined_hdl(UserContext* u_context)
{
	int ret;

	TracePrintf(2,"trap_kundefined_hdl: entered.\n");
	TracePrintf(0,"trap_kundefined_hdl: error: the kernel has been interrupted by an \
undefined trap. \n");

	ret = sys_Exit(u_context,1);
	TracePrintf(3,"trap_undefined_hdl: sys_Exit returned with %d\n", ret);
	u_context->regs[0] = ret;
	TracePrintf(2,"trap_undefined_hdl: returning...\n");
}

void trap_kernel_hdl(UserContext* u_context)
{
	TracePrintf(2,"trap_kernel_hdl: entered.\n");
	int id;
	int ret;
	switch (u_context->code) {
		case YALNIX_BRK:ret = sys_Brk(u_context);break;
		case YALNIX_FORK:ret = sys_Fork(u_context);break;
		case YALNIX_EXEC:ret = sys_Exec(u_context);break;
		case YALNIX_EXIT:ret = sys_Exit(u_context,0);break;	// 0: normal. 1: abnormal
		case YALNIX_WAIT:ret = sys_Wait(u_context);break;
		case YALNIX_DELAY:ret = sys_Delay(u_context);break;
		case YALNIX_GETPID:ret = sys_Getpid();break;
		case YALNIX_TTY_READ:ret = sys_TtyRead(u_context);break;
		case YALNIX_TTY_WRITE:ret = sys_TtyWrite(u_context);break;			
		case YALNIX_PIPE_INIT:ret = sys_PipeInit(u_context);break;
		case YALNIX_PIPE_READ:ret = sys_PipeRead(u_context);break;
		case YALNIX_PIPE_WRITE:ret = sys_PipeWrite(u_context);break;	
		case YALNIX_LOCK_INIT:ret = sys_LockInit(u_context);break;
		case YALNIX_LOCK_ACQUIRE:ret = sys_Acquire(u_context,NULL);break;
		case YALNIX_LOCK_RELEASE: id = u_context->regs[0];ret = sys_Release(id);break;   
		case YALNIX_CVAR_INIT:ret = sys_CvarInit(u_context);break;        
		case YALNIX_CVAR_SIGNAL:  ret = sys_CvarSignal(u_context);break;   
		case YALNIX_CVAR_BROADCAST:ret = sys_CvarBroadcast(u_context);break; 
		case YALNIX_CVAR_WAIT: ret = sys_CvarWait(u_context);break;
		case YALNIX_RECLAIM:ret = sys_Reclaim(u_context);break;
		case YALNIX_CUSTOM_0:ret = sys_Custom_0(u_context);break;	// sem_init
		case YALNIX_CUSTOM_1:ret = sys_Custom_1(u_context);break;	// sem_inc
		case YALNIX_CUSTOM_2:ret = sys_Custom_2(u_context);break;	// sem_wait
		default:
			TracePrintf(0,"trap_kernel_hdl: sorry, I can't handle it now...\n");
	}

	TracePrintf(3, "trap_kernel_hdl: ret = %d\n", ret);
	u_context->regs[0] = ret;
	TracePrintf(2,"trap_kernel_hdl: returning...\n");
}

void trap_clock_hdl(UserContext* u_context)
{
	TracePrintf(2,"trap_clock_hdl: entered.\n");
	update_sleep_list();		// someone may be sleeping in Wait() call
	do_scheduler(u_context);	// see if scheduling should be done
	TracePrintf(2,"trap_clock_hdl: returning...\n");
}

void trap_illegal_hdl(UserContext* u_context)
{
	int ret;
	TracePrintf(2,"trap_illegal_hdl: entered.\n");
	TracePrintf(0,"trap_illegal_hdl: illegal operation: process Abort\n");
	ret = sys_Exit(u_context,1);
	TracePrintf(3,"trap_illegal_hdl: sys_Exit returned with %d\n", ret);
	u_context->regs[0] = ret;
	TracePrintf(2,"trap_illegal_hdl: returning...\n");
}

void trap_memory_hdl(UserContext* u_context)
{

	int next_pn;	// page number specified by addr
	int pn_end;	// to which the stack grow
	int i, ret;

	TracePrintf(2,"trap_memory_hdl: entered.\n");
	TracePrintf(3,"trap_memory_hdl: code: %x\n", u_context->code);

	TracePrintf(3, "trap_memory_hdl: page0.pfn: %d, page0.prot: %d, page0.valid: %d\n",\
active_proc->u_page_table[0].pfn, active_proc->u_page_table[0].prot, active_proc->u_page_table[0].valid);

	TracePrintf(3, "trap_memory_hdl: PROT_READ: %d, PROT_WRITE: %d\n", PROT_READ, PROT_WRITE);

	pn_end = ((DOWN_TO_PAGE((*u_context).addr)-VMEM_1_BASE) >> PAGESHIFT ) -1; 

	TracePrintf(3,"trap_memory_hdl: addr: %p\n", u_context->addr);
	TracePrintf(3,"trap_memory_hdl: pn_end: %d, next_stack_pn: %d\n", pn_end, active_proc->next_stack_pn);

	if (pn_end <= active_proc->next_stack_pn &&	// stack growing and ...
		 pn_end > active_proc->next_heap_pn)	// not crashing heap
	{	
		TracePrintf(3, "trap_memory_hdl: entered if\n");
		// allocate frame and write pagetable
		for(i = active_proc->next_stack_pn; i > pn_end; i--) {
			active_proc->u_page_table[i].valid = 1;
			active_proc->u_page_table[i].prot = PROT_WRITE | PROT_READ;
			if (0 == (active_proc->u_page_table[i].pfn = grab_a_frame())){
				TracePrintf(0,"trap_memory_hdl: error: not enough mem\n");
				// mark: then what to do?
			}
		}
		active_proc->next_stack_pn = pn_end;
		// no need to flush here, but nor sure...
		// WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	}
	else{
		TracePrintf(0,"trap_memory_hdl: error: illegal memory access. exit.\n");
		ret = sys_Exit(u_context, 1);
		TracePrintf(3,"trap_memory_hdl: sys_Exit returned with %d\n", ret);
		u_context->regs[0] = ret;
	}
	TracePrintf(2,"trap_memory_hdl: returning...\n");
	
}

void trap_math_hdl(UserContext* u_context)
{
	int ret;

	TracePrintf(2,"trap_math_hdl: entered.\n");
	TracePrintf(0,"trap_math_hdl: error: illegal math instruction such as division by zero.\
 current process will be aborted. \n");
	TracePrintf(0,"trap_math_hdl: error code: %x\n", u_context->code);

	ret = sys_Exit(u_context,1);
	TracePrintf(3,"trap_math_hdl: sys_Exit returned with %d\n", ret);
	u_context->regs[0] = ret;

	TracePrintf(2,"trap_math_hdl: returning...\n");
}

void trap_tty_receive_hdl(UserContext* u_context)
{
	int i;
	int len;
	node* tmp;
	pcb_t* pcb;
 
	int tty_id = u_context->code;

	TracePrintf(2,"trap_tty_receive_hdl: entered.\n");

	// retrieving data from hardware
	len = TtyReceive(tty_id, receive[tty_id], TERMINAL_MAX_LINE);
	TracePrintf(3, "trap_tty_receive_hdl: terminal %d received %d bytes.\n", tty_id, len);
	if (len < TERMINAL_MAX_LINE) {
		receive[tty_id][len] = '\0';
		string_len = len+1;
	}
	else
		TracePrintf(3, "trap_tty_receive_hdl: len == TERMINAL_MAX_LINE\n");

	TracePrintf(3, "trap_tty_receive_hdl: received str: %s\n", receive[tty_id]);
	 
	// check if anyone if waiting for this
	i = 1;
	tmp = list_index(&io_list, i);
	while(tmp != NULL){
		pcb = (pcb_t*)(tmp->data);
		// if a match is found, unblock that process with right data 
		if ((pcb->state == BLOCKED_R) && (tty_id == pcb->tty_id)) {	
			tmp = list_remove(&io_list, i);
			if(tmp == NULL)
				TracePrintf(0, "trap_tty_receive_hdl: error in list_remove.\n");
			list_insert(&ready_queue, ready_queue.len+1, (void*)pcb);
			break;
		}
	}
	
	TracePrintf(2,"trap_tty_receive_hdl: returning...\n");
}

void trap_tty_transmit_hdl(UserContext* u_context)
{
	int i;
	node* tmp;
	pcb_t* pcb;
	int tty_id = u_context->code;

	TracePrintf(2,"trap_tty_transmit_hdl: entered.\n");
	//TracePrintf(1,"trap_tty_transmit_hdl: entered. tty_id: %d\n", tty_id);

	busy_terminal[tty_id] = 0;
	// check if anyone if waiting for this
	i = 1;
	tmp = list_index(&io_list, i);
	while(tmp != NULL){
		pcb = (pcb_t*)(tmp->data);
		// if a match is found, unblock that process with right data 
		if ((pcb->state == BLOCKED_W) && (tty_id == pcb->tty_id)) {	
			tmp = list_remove(&io_list, i);
			if(tmp == NULL)
				TracePrintf(0, "trap_tty_transmit_hdl: error in list_remove.\n");
			list_insert(&ready_queue, ready_queue.len+1, (void*)pcb);
			break;
		}
		i++; tmp = list_index(&io_list, i);
	}

	TracePrintf(2,"trap_tty_transmit_hdl: returning...\n");
}

void trap_disk_hdl(UserContext* u_context)
{
	TracePrintf(2,"trap_disk_hdl: entered.\n");
	TracePrintf(0,"trap_disk_hdl: cannot handle this.\n");
	TracePrintf(2,"trap_disk_hdl: returning...\n");
}







