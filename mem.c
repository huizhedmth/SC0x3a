#include "mem.h"

#include "kernel.h"	
#include "hardware.h"

/* return frame number upon success, otherwise return 0 */
int grab_a_frame()
{
	TracePrintf(2, "grab_a_frame: entered.\n");	
	// grab a free frame from head
	int ret;
	node* tmp;

	tmp = list_remove(&free_frames, 1);
	if(tmp == NULL){
		TracePrintf(0, "grab_a_frame: internal error in list.\n");	
		return 0;
	}
	ret = (int)(tmp->data);
	free(tmp);

	TracePrintf(2, "grab_a_frame: returning...\n");
	return ret;
}


