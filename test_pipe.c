
#include "yalnix.h"
#ifdef LINUX
#define NULL 0
#endif

int main(int argc, char* argv[]){
	
	// test 1 : pipe, lock and cvar
	int lock_id, cvar_id, pipe_id;
	int pid,status;
	int condition=0;
	char *test = "Yalnix Works";
	char *buffer = (char*)malloc(1024);

	TracePrintf(1, "init main: test pipe, lock, cvar.\n");
	LockInit(&lock_id);
	CvarInit(&cvar_id);
	PipeInit(&pipe_id);
	TracePrintf(1, "init main: Lockid %d.\n", lock_id);
	TracePrintf(1, "init main: Cvarid %d.\n", cvar_id);
	TracePrintf(1, "init main: Pipeid %d.\n", pipe_id);

	
	pid = Fork();
	if (pid == 0) {	
			TracePrintf(1,"init main: child \n");
			Acquire(lock_id);
			TracePrintf(1,"init main: child acquire the lock\n");
			condition=1;
			TracePrintf(1,"init main: child condition %d\n",condition);
			PipeWrite(pipe_id, &condition,sizeof(int));
			TracePrintf(1,"init main: child change the condition and write it to pipe\n");
			TracePrintf(1,"init main: child cvar signal\n");
			CvarSignal(cvar_id);
			Release(lock_id);
			TracePrintf(1,"init main: child write to pipe: %s\n",test);
			PipeWrite(pipe_id,test,20);
			TracePrintf(1,"init main: child release the lock\n");
			Exit(0);

	}
	else{
		TracePrintf(1,"init main: parent\n");
		Acquire(lock_id);
		TracePrintf(1,"init main: parent acquire the lock\n");
		while(!condition){
			TracePrintf(1,"init main: parent cvar wait, condition %d\n",condition);
			CvarWait(cvar_id,lock_id);
			PipeRead(pipe_id,&condition,sizeof(int));
			TracePrintf(1,"init main: parent read the condition from pipe, condition %d\n",condition);
		}
		TracePrintf(1,"init main: parent wake up\n");
		Release(lock_id);
		TracePrintf(1,"init main: parent release the lock\n");		
		PipeRead(pipe_id,buffer,20);
		TracePrintf(1,"init main: parent read from pipe: %s\n",buffer);
		
	}
	
	Reclaim(lock_id);
	Reclaim(cvar_id);
	Reclaim(pipe_id);
	Exit(0);
	
 
}










