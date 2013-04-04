/* list.h
 * A simple implementation of linked list used by the kernel
 * Huizhe Li, 10/20/2012
 */

/* usage:
 * the list has a fake "head" node, so you have to malloc for head
 * and init len to 0 upon creation
 * elements are indexed from 1 to "len"
 */
 
#ifndef LIST_H
#define LIST_H

typedef struct link_node{
	void* data;
	struct link_node* next;
}node;


typedef struct linked_list{
	node* head;
	int len;
}list;

/* insert an elem at location. return 0 upon success, otherwise ERROR */
int list_insert(list* lst, int location, void* data) ;

/* remove an elem at location. return removed node upon success, otherwise ERROR */
node* list_remove(list* lst, int location);
	
/* fine an elem at location. return pointer to the elem */
node* list_index(list* lst, int location);

/* destroy the list. return 0 upon success, otherwise ERROR */
int list_delete(list* lst);	
			
#endif // LIST_H
























