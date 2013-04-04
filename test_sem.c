#include <hardware.h>
#include <yalnix.h>
#include <stdlib.h>
#include <assert.h>


// test sem
int sem1, sem2;
int foo = 0;
int count;


int main(void){

	int i;
	int pid;
	int status;
	int rc;

	Custom0(foo, foo, foo, foo);

	sem1 = Custom0(foo, foo, foo, foo);
	if (sem1 == ERROR)
		TracePrintf(0, "failed creating sem\n");
	TracePrintf(0, "got a sem, id = %d\n", sem1);
	sem2 = Custom0(foo, foo, foo, foo);
	if (sem2 == ERROR)
		TracePrintf(0, "failed creating sem\n");
	TracePrintf(0, "got another sem, id = %d\n", sem2);


	for(i = 0; i < 3; i ++){	// create 3 children waiting on sem 1
		pid = Fork();
		if (pid == 0){
			rc = Custom2(sem1, foo, foo, foo);
			if (rc == ERROR)
				TracePrintf(0, "test main: error 2\n");
			TracePrintf(0, "sem1: %d is back!\n", GetPid());
			Delay(10);
			Exit(0);
		}		
	}	

	for(i = 0; i < 3; i ++){	// create 3 children waiting on sem 2
		pid = Fork();
		if (pid == 0){
			rc = Custom2(sem2, foo, foo, foo);
			if (rc == ERROR)
				TracePrintf(0, "test main: error 2\n");			
			TracePrintf(0, "sem2: %d is back!\n", GetPid());
			Delay(10);
			Exit(0);
		}		
	}	

	Delay(3);
	for(i = 0; i < 3; i ++){	// wake them up
		rc = Custom1(sem1, foo, foo, foo);
		if (rc == ERROR)
			TracePrintf(0, "test main: error 1\n");
		rc = Custom1(sem2, foo, foo, foo);
		if (rc == ERROR)
			TracePrintf(0, "test main: error 1\n");
	}

	while( rc != ERROR){
		rc = Wait(&status);
		TracePrintf(0, "rc = %d, status = %d\n", rc, status);
	}	
	TracePrintf(0, "parent back!\n");
	Exit(0);
}





