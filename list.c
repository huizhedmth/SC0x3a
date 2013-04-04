/* list.h
 * A simple implementation of linked list used by the kernel
 * Huizhe Li, 10/20/2012
 */

#include "list.h"

#include "yalnix.h"

#ifndef NULL
#define NULL 0
#endif

int list_insert(list* lst, int location, void* data) 
{
	int i;
	node *curr, *tmp;

	if (lst == NULL)
		return ERROR;

	if (location > ((*lst).len+1) || location < 1)
		return ERROR;

	node* new_node =(node*)malloc(sizeof(node));

	if (new_node == NULL)
		return ERROR;

	curr = (*lst).head;
	for (i = 0; i < location-1; i++){
		curr = (*curr).next;
	}

	tmp = (*curr).next;
	(*curr).next = new_node;
	(*new_node).data = data;
	(*new_node).next = tmp;
	(*lst).len++;

	return 0;
}
	 
node* list_remove(list* lst, int location)
{
	int i;
	node *curr, *tmp;
	
	if (lst == NULL)
		return NULL;
	if (location > (*lst).len || location < 1){
		TracePrintf(1, "list: location %d, len %d\n",location,(*lst).len);
		TracePrintf(1, "list: there is nothing to remove~\n");
		return NULL;
	}	
	//TracePrintf(1, "list: there is something to remove~\n");
	//TracePrintf(1, "list: before moving: len: %d, location: %d, tmp: %p\n", lst->len, location, tmp);
	curr = (*lst).head;
	for (i = 0; i < location-1; i++){
		curr = (*curr).next;
	}
	tmp = (*curr).next;
	(*curr).next = (*tmp).next;
	// mark: free(tmp);
	(*lst).len--;
	//TracePrintf(1, "list: after moving: len: %d, location: %d, tmp: %p\n", lst->len, location, tmp);
	return tmp;
}
	
node* list_index(list* lst, int location)
{
	int i;
	node* curr;

	if (lst == NULL)
		return NULL;

	if (location > (*lst).len || location < 1)
		return NULL;
	
	curr = (*lst).head;
	for (i = 0; i < location ; i++){
		curr = (*curr).next;
	}
	return curr;
}

int list_delete(list* lst)
{
	int i;
	node *curr, *nxt;
	
	if (lst == NULL)
		return -1;

	nxt = (*lst).head;
	for (i = 0; i < (*lst).len+1; i++){
		curr = nxt;
		nxt = (*nxt).next;
		free(curr); curr = NULL;
	}
	(*lst).len = 0;

	return 0;
}
		
			
























