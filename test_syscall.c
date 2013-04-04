/* test_syscall.c
 *  
 * PART 1: test syscall functionality provided by our sulotion 
 * PART 2: test robustness of syscall by passing various bad arguments
 *
 * Note: cvar and pipe are tested in another program (test_syscall2)
 */

#include <hardware.h>
#include <yalnix.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char* argv[])
{
	int pid;
	char* null_arg = NULL;
	char** args;
	char* arg[3];
	char* arg1 = "exec is ";
	char* arg2 = "a ";
	char* arg3 = "good boy";
	int status;
	int rc;

	char* read = (char*)malloc(100);
	char* prompt = "init main: in while\n";
	int len;
	int i;

	char* str1 = "Please enter some characters:";
	char* str2 = "check it out in terminal 3\n";
	char* str3 = "You just entered:";
	char* str4 = "going to write 10000 characters\n";

	arg[0] = arg1;
	arg[1] = arg2;
	arg[2] = arg3;
	args = arg;

	pid = GetPid();
	TtyPrintf(0, "Hi, I'm process %d, now I'm going to perform a series of tests.\n", pid);
	TtyPrintf(0, "PART 1: syscall funcionality\n");
	Delay(6);

//--------------------------------- PART 1 -------------------------------//


	TtyPrintf(0, "PART 1, test 1...\n");
	TtyPrintf(0, "test Fork, Exec, Exit, Wait and Delay\n");
	Delay(6);

	// test 1: Fork, Exec, Exit, Wait and Delay
	pid = Fork();
	if (pid == 0){	// child
		TtyPrintf(0, "child is going to Exec...\n");
		Delay(6);
		Exec("exec", &null_arg);
		Exit(21); 
	}else{		// parent
		rc = Wait(&status);
		if (rc == ERROR) 
			TtyPrintf(0, "error waiting child.\n");
		TtyPrintf(0, "child process %d returned with %d\n\n", rc, status);
	}





	TtyPrintf(0, "PART 1, test 2...\n");
	TtyPrintf(0, "test TtyRead and TtyWrite\n\n");
	Delay(6);
		
	// test 2: TtyRead and TtyWrite

	TtyWrite(2, str1, strlen(str1));

	len = TtyRead(2, read, 512);
	TtyWrite(2, str2, strlen(str2));
	TtyWrite(3, str3, strlen(str3));
	TtyWrite(3, read, len);
	
	for (i = 0; i < 10000; i ++){
		read[i] = 'a';
	}
	TtyWrite(0, str4, strlen(str4));
	len = TtyWrite(2, read, 10000);

//--------------------------------- PART 2 -------------------------------//

	TtyPrintf(0, "PART 2: syscall robustness\n");

	// test 1: Exec
	TtyPrintf(0, "PART 2, test 1...\n");
	TtyPrintf(0, "bad arguments for Exec:)\n");

	// non-existing program
	pid = Fork();
	if (pid == 0){
		TtyPrintf(0, "chile is going to Exec a non-existing program...\n");
		Delay(6);
		rc = Exec("non-existing program", &null_arg);	
		TtyPrintf(0, "Exec returned with: %d\n", rc);
		Exit(-1);
	}
	Wait(&status);
	TtyPrintf(0, "child returned with %d\n", status);
	
	Delay(6);
	// bad cmd arguments
	pid = Fork();
	if (pid == 0){
		TtyPrintf(0, "chile is going to Exec a program with bad arguments...\n");
		Delay(6);
		rc = Exec("exec", NULL);
		TtyPrintf(0, "Exec returned with: %d\n", rc);
		Exit(-1);
	}
	Wait(&status);
	TtyPrintf(0, "child returned with %d\n\n", status);


	Delay(6);




	// test 2: Wait 

	TtyPrintf(0, "PART 2, test 2...\n");
	TtyPrintf(0, "bad arguments for Wait:)\n");
	pid = Fork();
	if (pid == 0){
		Delay(20);
		Exit(0);
	}

	rc = Wait(NULL);	
	if (rc == ERROR)
		TtyPrintf(0, "don't pass NULL to wait:)\n");
		
	Delay(4);	
	rc = Wait((int*)(0x01));
	if (rc == ERROR)
		TtyPrintf(0, "kernel: wanna mess me up?\n");
 	
	Delay(4);		
	rc = Wait((int*)(VMEM_1_BASE + 0x20000));		
	if (rc == ERROR)
		TtyPrintf(0, "be careful with your pointer!\n\n");

	Delay(6);


	// test 3: Delay
	TtyPrintf(0, "PART 2, test 3...\n");
	TtyPrintf(0, "bad arguments for Delay:)\n");
	TtyPrintf(0, "this is not bad, though. Delay(0)!\n");
	rc = Delay(0);
	if (rc == 0)
		TtyPrintf(0, "Delay(0) returned immediately!\n");

	Delay(6);
	TtyPrintf(0, "I want to Delay(-10), um...\n");
	Delay(3);
	rc = Delay(-10);
	if (rc == ERROR)
		TtyPrintf(0, "want time travel? well... wake up.\n\n");

	Delay(6);


	// test 4: TtyRead
	TtyPrintf(0, "bunch of bad arguments coming for TtyRead and TtyWrite...\n");
	Delay(1);

	rc = TtyRead(-1, read, 20);
	if (rc == ERROR)
		TtyPrintf(0, "where are you going?\n");

	Delay(5);
	rc = TtyRead(3, read, -1);
	if (rc == ERROR)
		TtyPrintf(0, "-1? do you want to Write instead?\n");

	Delay(5);
	rc = TtyRead(1, (void*)(0x04), 20);
	if (rc == ERROR)
		TtyPrintf(0, "kernel is not happy now...\n");

	Delay(5);
	TtyRead(1, (void*)(VMEM_1_BASE + 0x20000), 20);
	if (rc == ERROR)
		TtyPrintf(0, "be careful with your pointer!\n\n");

	Delay(6);


	// test 5: Tty Write
	TtyPrintf(0, "maybe TtyWrite is weaker? let's see...\n");
	Delay(6);
	rc = TtyWrite(-1, read, 20);
	if (rc == ERROR)
		TtyPrintf(0, "where are you going?\n");

	Delay(5);
	rc = TtyWrite(3, read, -1);
	if (rc == ERROR)
		TtyPrintf(0, "-1? again?\n");

	Delay(5);
	rc = TtyWrite(1, (void*)0x04, 20);
	if (rc == ERROR)
		TtyPrintf(0, "kernel is in fury. run for your life!\n");

	Delay(5);
	rc = TtyWrite(1, (void*)(VMEM_1_BASE + 0x20000), 20);
	if (rc == ERROR)
		TtyPrintf(0, "another illegal address...\n");
	

	TtyPrintf(0, "Survived test! Exiting...\n");
	Exit(0);
}









