/*
 *  }:->
 */

#include <hardware.h>
#include <yalnix.h>

#include <stdlib.h>
#include <assert.h>

// Waits for a cvar signal, prints a message
void Bouncer(int cvar, int mutex)
{
    TtyPrintf(1, "Bouncer here\n");
    while (1)
    {
	TtyPrintf(1, "Waiting for ping...");
	Acquire(mutex);
	CvarWait(cvar, mutex);
	Release(mutex);
	TtyPrintf(1, "boing!\n");
    }
    TtyPrintf(1, "*** SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}



// Keeps breaking in
void ThatAnnoyingGuy(void)
{
    while (1)
    {
	TtyPrintf(1, "Hey!  Hey!  You remember that time when...\n");
	Delay(1);
	TtyPrintf(1, "Hi!  I'm that annoying guy that all the other processes hate!\n");
	Delay(1);
	TtyPrintf(1, "Hehehehehehehehehehe!\n");
	Delay(1);
	TtyPrintf(1, "Why did the chicken cross the road?  Ha ha...\n");
	Delay(1);
	TtyPrintf(1, "42!!!\n");
	Delay(1);
	TtyPrintf(1, "Get it?  42?  Get it?  Huh?  Hey, you're not listening...\n");
	Delay(1);
	TtyPrintf(1, "You know what would be a really scary Halloween costume?\n");
	Delay(1);
	TtyPrintf(1, "A null pointer!  Ha ha ha.\n");
	Delay(1);
	TtyPrintf(1, "My while loop is over, so I'm goint to repeat all my lame jokes now.\n");
	Delay(1);
    }
    TtyPrintf(1, "*** I SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}



void MallocMan(void)
{
    int npg;
    void *ptr;

    while (1)
    {
	npg = rand() % 21;
	TtyPrintf(2, "MallocMan: malloc'ing %d pages\n", npg);
	ptr = malloc(PAGESIZE*npg);
	Delay(3);	
	TtyPrintf(2, "MallocMan: freeing the stuff I just malloc'ed\n");
	free(ptr);
    }
    TtyPrintf(1, "*** I SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}

void SonarGuy(int cvar, int mutex)
{
    TtyPrintf(0, "SonarGuy here (my job is to signal Bouncer)\n");
    while (1)
    {
	TtyPrintf(0, "Signaling Bouncer...");
	Delay(2);
	Acquire(mutex);
	CvarSignal(cvar);
	Release(mutex);
	TtyPrintf(0, "ping!\n");
    }
    TtyPrintf(1, "*** I SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}



void GarbageMan(void)
{
    int i, j, sentLen, wordLen;
    char punc[] = ".!?;:";
    char ch;

    TtyPrintf(3, "Hi, I'm the garbage man!\n");
    Delay(6);
    TtyPrintf(3, "All I do is spew garbage!  Watch!\n");
    while (1)
    {
	sentLen = rand() % 8;
	Delay(rand() % 7);
	for (i = 0; i < sentLen; i++)
	{
	    wordLen = rand() % 13;
	    for (j = 0; j < wordLen; j++)
	    {
		ch = 'a' + rand() % 26;
		TtyWrite(3, &ch, 1);
	    }
	    if (i < (sentLen - 1)) {
		ch = ' ';    
		TtyWrite(3, &ch, 1);
	    }
	}
	TtyWrite(3, &punc[rand() % 5], 1);
	ch = ' ';
	TtyWrite(3, &ch, 1);
    }
    TtyPrintf(1, "*** I SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}


int main(void)
{
    int pid;
    int r, cvar, mutex;

    char buf[100];

    r = CvarInit(&cvar);
    assert(r != ERROR);
    r = LockInit(&mutex);
    assert(r != ERROR);

    TtyPrintf(0, "foo Enter 'go' to go:\n");
    do {
	TtyRead(0, buf, 100);
    } while (buf[0] != 'g' || buf[1] != 'o');

	
    pid = Fork();
    if (pid == 0)
	Bouncer(cvar, mutex);

	
    pid = Fork();
    if (pid == 0)
	ThatAnnoyingGuy();

	
    pid = Fork();
    if (pid == 0)
	SonarGuy(cvar, mutex);
  
    pid = Fork();
    if (pid == 0)
	MallocMan();


    pid = Fork();
    if (pid == 0)
	GarbageMan();

    while(1)
    {
	Delay(10);
    }

    TtyPrintf(0, "*** I SHOULDN'T BE HERE!!!\n");
    return -1;
}
