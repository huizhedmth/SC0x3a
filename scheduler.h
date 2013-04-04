#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "hardware.h"
#include "kernel.h"
#include "list.h"

#define PIPE_BUF_SIZE 4096

typedef struct sleep_info {
  int ticks_left;
  pcb_t *pcb;
} sleep_info;

typedef struct dead_info{
  int ppid;
  int pid;
  int status;
}dead_info;

typedef struct pipe_info{
	int pipeid;
	int cur_len;
	void* pipe_buffer;
	list pipe_wait_list;
}pipe_info;


typedef struct lock_info{
  int lockid;
  int locked;
  int pid;
  list lock_wait_list;//pcb_t as data
}lock_info;

typedef struct cvar_info{
	int cvarid;
	list cvar_wait_list;//pcb_t as data
}cvar_info;

typedef struct reclaim_info{
	int id;
	int type;//0 for pip, 1 for lock, 2 for cvar 
}reclaim_info;


int update_sleep_list();
int insert_sleep_list(pcb_t *p, int clock_ticks);
int insert_dead_list(int ppid, int pid, int status);
int do_scheduler(UserContext *u_context);
int destroy_pipe_info(int pipe_id);
int destroy_lock_info(int lock_id);
int destroy_cvar_info(int cvar_id);

/* kernel context switch funtion */
KernelContext* switch_kernel_context(	KernelContext* kcs_in,	
					void* p_old,
					void* p_new
					);

/* populate a pcb with a copy of initial kernel stack and context */ 
KernelContext* init_proc_kernel	(	KernelContext* kcs_in,	
					void* p_pcb,
					void* not_used
					);

/* save a copy of initial kernel stack and context as global vars */ 
KernelContext* save_initial_kernel(	KernelContext* kcs_in,	
					void* not_used,
					void* useless
					);

/* return kernel to initial state */ 
KernelContext* return_initial_state(	KernelContext* kcs_in,	
					void* not_used,
					void* useless
					);


/* do context switch */
void context_switch(UserContext* uctxt, pcb_t* curr, pcb_t* next);

#endif // SCHEDULER
