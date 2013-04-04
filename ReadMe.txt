/** 	1. Achievements
 */


The kernel meets basic requirements in the manual, has passed torture, cp5_test, cp6_test and some tests written by ourselves.



/** 	2. Bugs
 */

TtyWrite behaves weird in scheduling with Delay, but we have not been able to fix this. The bug probably lies in function TtyWrite, therefore some tests use TracePrintf to output to show that other parts are working.



/** 	3. Extra features
 */

Semaphores.



/** 	4. Specification clarifications
 */

## 1. KernelStart ##

The kernel automatically seeks executable "idle" to load as the idle process, and prints error information if fails. 

The user must specify, using the first command line argument, which program the kernel should run as the "initial" process, and successive command line arguments as the program's arguments. The kernel prints error information if this fails. 


## 2. Disk Trap ##

The kernel simply does a TracePrintf, reporting that it does not deal with this type of trap.


## 3. Pipe ##

The kernel is hard-coded to have a 4KB pipe buffer. Also, pipes do not have owners, which means that given the right ID, every process is allowed to access a specific pipe.


## 4. Error code ##

We do not use error codes other than the value ERROR. Instead, error information is printed by TracePrintf.


## 5. Semaphores (extra feature) ##

The three Custom syscalls are used to support semaphores. Semaphores work independently with Locks and Cvars, so a semaphore and a lock(or a cvar) may have the same ID. A semaphore can not be reclaimed.

Custom0(int foo, int foo, int foo, int foo) works as sem_init, which ingores all the four parameters. On success, it returns the ID of the semaphore created; otherwise it returns the value ERROR.

Custom1(int ID, int foo, int foo, int foo) works as sem_signal. It takes its first parameter as the semaphore ID, and ignores the remaining parameters. It returns 0 upon success, and the value ERROR if 
error occurs.

Custom2(int ID, int foo, int foo, int foo) works as sem_wait. It takes its first parameter as the semaphore ID, and ignores the remaining parameters. It returns 0(the time it wakes up, if blocked earlier) upon success, and the value ERROR if error occurs.



/** 	5. Building yalnix and test cases
 */

## 1. Building Yalnix ##

$ make


## 2. Building testing programs ##

$ make -f maketest. It works only if yalnix has been built. 




/** 	6. Running Test cases
 */

## 1. Running options ##

$ yalnix -t -s -lk 1 -lu 1 NameOfTest arg1 arg2 (...)

TracePrintf level works as follows:
level 0: any error information.

level 1: reserved, currently only prints in the idle process.

level 2: shows the flow of data and control in kernel-land. 

level 3: debugging. This level prints values of local variables or simply marks to help dubugging the kernel.

Normally level 1 is used.


## 2. Test cases  ##

We have run cp5_test, cp6_test and torture to test the kernel. None of these programs actually use command line arguments.

Our own test cases are listed as follows:

test_syscall -- perform a series of tests to show functionality of syscalls, including Fork, Exec, Exit, Wait, Delay, TtyRead and TtyWrite. Bad arguments are also tested.

test_sem, test_cvar, test_pipe -- test remaining syscalls.

test_mem -- test growth of stack and heap. This test takes one argument, which should be 1 or 2, indicating stack test and heap test, respectively. 



/** 	Appendix: List of files
 */

## Kernel Source Code ##

kernel.c list.c mem.c scheduler.c syscall.c trap.c loadprogram.c
kernel.h list.h mem.h scheduler.h syscall.h trap.h loadprogram.h

## User Program Source Code ##

cp5_test.c cp6_test.c torture.c exec.c idle.c test_cvar.c test_mem.c test_pipe.c test_sem.c 
test_syscall.c 

## makefile ##

Makefile maketest

## ReadMe ##

ReadMe.txt
