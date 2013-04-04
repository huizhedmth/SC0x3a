#include "yalnix.h"
#ifdef LINUX
#define NULL 0
#endif

int main(){
	TracePrintf(0, "idle main: entered.\n");
	while(1){
		TracePrintf(0, "idle main: in while\n");
		Pause();
	}
}
