#ifndef SYSCALL_H
#define SYSCALL_H

#include "hardware.h"

int sys_Brk(UserContext* u_context);	// Brk
int sys_Fork(UserContext* u_context);	// Fork
int sys_Exec(UserContext* u_context);	// Exec
int sys_Exit(UserContext* u_context, int mode);   // Exit
int sys_Wait(UserContext* u_context);   // Wait
int sys_Delay(UserContext* u_context);  // Delay
int sys_Getpid();                       // Getpid
int sys_TtyRead(UserContext* u_context);	// TtyRead
int sys_TtyWrite(UserContext* u_context);       // TtyWrite
int sys_LockInit(UserContext* u_context);	// LockInit
int sys_Acquire(UserContext *u_context, int id); // Acquire 
int sys_Release(int lock_id);	// Release
int sys_CvarInit(UserContext* u_context);	// CvarInit
int sys_CvarSignal(UserContext *u_context);	// CvarSignal
int sys_CvarBroadcast(UserContext *u_context);	// CvarBroadcast
int sys_CvarWait(UserContext* u_context);	// CvarWait
int sys_Reclaim(UserContext* u_context); 	// Reclaim
int sys_PipeInit(UserContext *u_context);
int sys_PipeRead(UserContext *u_context);
int sys_PipeWrite(UserContext *u_context);
#endif // SYSCALL_H
