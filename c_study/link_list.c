/*  question 1: through adjust pointer(not change data)to exchange two neighbouring element,use: a.singly-linked list. b.Doubly-linked list.
 *
 */

#include "link_list.h"


static inline LINK_LIST *headCreate(void);/*create a header node with value 0 and pointer NULL*/

static inline LINK_LIST *nodeCreate(int value);/*create a node with value 0,but previous and next pointer is NULL*/

int isEmpty(const LINK_LIST *HEAD);/*return 0 means not empty,return -1 means empty*/




static void listInsert(LINK_LIST *HEAD,int position,int value);
static LINK_LIST * findPosition(LINK_LIST *HEAD,const int position);
static void listPrint(LINK_LIST *HEAD,const int position);


static void sortList(LINK_LIST *HEAD)
{
	
}



/*
 * 	if position is 0.then destory the whole linklist,if position > 0.then delete the position
 */
static void elementDel(LINK_LIST *HEAD,int position)
{
	if (position < 0){
		printf("you have input a wrong position\n");
		return ;
	}

	LINK_LIST *tmp;

	if (position == 0){
		printf("delete the whole link\n");
		tmp=HEAD->next;
		free(HEAD->next);
		while(tmp->next != NULL){
			free(tmp->next);
			printf("%p\n",tmp->next);
			tmp = tmp->next;
		}
	}else{
		int flags = 0;
		while((flags < position) && (HEAD->next != NULL)){
			HEAD = HEAD->next;
			flags++;
		}

		if (HEAD->next == NULL){
			printf("the position is out of range,can not delete it\n");
			return ;
		}else{
			HEAD->previous->next=HEAD->next;
			HEAD->next->previous=HEAD->previous;
			free(HEAD);
			return;
		}
	}
return ;

}

int main(int argc, char const *argv[])
{
	LINK_LIST *head = headCreate();
	// printf("head value is %d\n",head->value);

	listInsert(head,0,12);
	listInsert(head,0,11);
	listInsert(head,0,5);
	listInsert(head,0,21);
	listInsert(head,9,19);
	listInsert(head,4,13);
	listInsert(head,3,13);


	isEmpty(head);

	printf("the whole linklist is :head");
	listPrint(head,0); //print the whole node
	listPrint(head,3); //print the value of node 3

	elementDel(head,6);//delete a node that out of range


	// elementDel(head,-5);//delete node 3,and show the list
	elementDel(head,3.3);//delete node 3,and show the list
	printf("\ndelete node 3,the rest is: head");
	listPrint(head,0);

	elementDel(head,0);//delete the whole link
	printf("head");
	listPrint(head,0);	

	return 0;
}


static inline LINK_LIST *headCreate(void)
{
	LINK_LIST *HEAD = (LINK_LIST *)malloc(strLen);
	memset(HEAD,0,strLen);
	return HEAD;
}

static inline LINK_LIST *nodeCreate(int value)
{
	LINK_LIST *node = (LINK_LIST *)malloc(strLen);
	node->value = value;
	node->previous = NULL;
	node->next = NULL;
	return node;
}

int isEmpty(const LINK_LIST *HEAD)
{
	if (HEAD->next == NULL){
		printf("the linklist is empty\n");
		return -1;
	}
	printf("the linklist is not empty\n");
	return 0;
}


static void listInsert(LINK_LIST *HEAD,int position,int value)
{
	//position = 0 means insert in the end of the link
	if (position < 0){
		printf("you have select a wrong position!\n");
		return;
	}
	if (position == 0){
		while(HEAD->next != NULL){
			HEAD = HEAD->next;
		}
		HEAD->next = nodeCreate(value);
		HEAD->next->next = NULL;
		HEAD->next->previous = HEAD;
		return;
	}

	int flags = 0;
	while(flags<position && HEAD->next != NULL){
		flags++;
		HEAD = HEAD->next;
	}
	if (HEAD->next == NULL){
		printf("the position %d is out of range of the list\n",position);
		return ;
	}else{
		LINK_LIST *tmp;
		tmp = HEAD->next;
		HEAD->next = nodeCreate(value);
		HEAD->next->next = tmp;
		tmp->previous = HEAD->next;
		HEAD->next->previous = HEAD;
		return;
	}
}

static LINK_LIST * findPosition(LINK_LIST *HEAD,const int position)
{
// 	static int flags=0;
// 	LINK_LIST *tmp;
// //set a tmp pointer to receive the final position
// 	if (flags < position){
// 		flags++;
// 		tmp = findPosition(HEAD->next,position);
// 	}else{
// 		return HEAD;
// 	}
// 	return tmp;

/* a more effective way*/

	int flags = 0;
	while(flags < position && HEAD->next != NULL){
		flags++;
		HEAD = HEAD->next;
	}

	if (HEAD->next == NULL){
		printf("the position is out of range!\n");
		return (LINK_LIST *)NULL;
	}else{
		return HEAD;
	}
}



static void listPrint(LINK_LIST *HEAD,const int position)
{
	//position = 0 means print all the node 
	if (position == 0){
		// if(HEAD->next != NULL){
		// 	printf("->%d",HEAD->next->value);
		// 	listPrint(HEAD->next,0);
		// }else{
		// 	printf("\n");
		// 	return ;	
		// }

/*use this more effective*/
		while(HEAD->next != NULL){
			printf("->%d",HEAD->next->value);
			HEAD = HEAD->next;
		}
		printf("\n");
		return ;
	}else{
// position != 0 means print the position's value
		LINK_LIST *tmp;
		tmp = findPosition(HEAD,position);
		printf("the value in position %d is %d\n",position,tmp->value);
		// printf("find the position\n");
	}
	return ;
}	