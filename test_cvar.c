#include <hardware.h>
#include <yalnix.h>
#include <stdlib.h>
#include <assert.h>


// test idle scheduling
int main(void){
	int pid,cvarid,lockid;
	TracePrintf(0, "test_temp main:\n");

	LockInit(&lockid);	
	CvarInit(&cvarid);
	
	pid = Fork();
	if(pid==0){
		TracePrintf(0, "child: acquire lock\n");
		Acquire(lockid);
		TracePrintf(0, "child: reclaim cvar\n");
		Reclaim(cvarid);
		TracePrintf(0, "child: exit\n");
		Exit(0);
	}
	else{
		TracePrintf(0, "parent: acquire\n");
		Acquire(lockid);
		TracePrintf(0, "parent: wait\n");
		CvarWait(cvarid,lockid);
		Release(lockid);
	}
	Exit(0);
}
