/*  question 1: through adjust pointer(not change data)to exchange two neighbouring element,use: a.singly-linked list. b.Doubly-linked list.
 *	question 2:学生注册问题的多重表实现:
 */

#include "link_list.h"


static inline LINK_LIST *headCreate(void);/*create a header node with value 0 and pointer NULL*/
static inline LINK_LIST *nodeCreate(int value);/*create a node with value 0,but previous and next pointer is NULL*/
static inline int isEmpty(const LINK_LIST *HEAD);/*return 0 means not empty,return -1 means empty*/
static inline void makeEmpty(LINK_LIST *HEAD);

void elementDel(LINK_LIST *HEAD,int position);
void listInsert(LINK_LIST *HEAD,int position,int value);
LINK_LIST * findPosition(LINK_LIST *HEAD,const int position);
void listPrint(LINK_LIST *HEAD,const int position);


void radix_sort(LINK_LIST *HEAD)//基数排序也叫卡片排序(card sort)
{
	
}


/*
 * 	if position is 0.then destory the whole linklist,if position > 0.then delete the position
 */


int main(int argc, char const *argv[])
{
	LINK_LIST *head = headCreate();
	// printf("head value is %d\n",head->value);

	listInsert(head,0,12);
	listInsert(head,0,11);
	listInsert(head,0,5);
	listInsert(head,0,21);
	listInsert(head,9,19);
	listInsert(head,3,13);
	listInsert(head,5,55);


	isEmpty(head);

	printf("the whole linklist is :head");
	listPrint(head,0); //print the whole node
	listPrint(head,5); //print the value of node 3

	elementDel(head,6);//delete a node that out of range


	// elementDel(head,-5);//delete node 3,and show the list
	elementDel(head,0);//delete node 3,and show the list
	printf("\ndelete node 0,the rest is: head");
	listPrint(head,0);	
	listPrint(head,3);

	return 0;
}


static LINK_LIST *headCreate(void)
{
	LINK_LIST *HEAD = nodeCreate(0);
	return HEAD;
}

static LINK_LIST *nodeCreate(int value)
{
	LINK_LIST *node = (LINK_LIST *)malloc(strLen);
	if (node == NULL){
		printf("ERROR!!! can not create node,maybe have not enough room\n");
		return NULL;
	}
	node->value = value;
	node->previous = NULL;
	node->next = NULL;
	return node;
}

static int isEmpty(const LINK_LIST *HEAD)
{
	if (HEAD->next == NULL){
		printf("the linklist is empty\n");
		return -1;
	}
	printf("the linklist is not empty\n");
	return 0;
}

static void makeEmpty(LINK_LIST *HEAD)
{
//==================================//
//	while(HEAD->next != NULL){		//
//		LINK_LIST *tmp;				//
//		tmp = HEAD->next;			//
//		free(HEAD->next);			//    this also work correctly
//		HEAD->next = NULL;			//
//		HEAD = tmp;					//
//	}								//
//==================================//


//======================2017.05.18 BEGIN======================================
	LINK_LIST *tmp,*p;
	tmp = HEAD->next;
	HEAD->next = NULL;
	while(tmp != NULL){
//==========================//	
//		free(tmp);			// this may not work for all system
//		tmp = tmp->next;	//	
//==========================//	
		p = tmp->next;
		free(tmp);
		tmp = p;
	}
//======================2017.05.18 END  ======================================
	return;
}


void elementDel(LINK_LIST *HEAD,const int position)
{
	if (position < 0){
		printf("you have input a wrong position %d\n",position);
		return ;
	}

	if (position == 0){
		printf("delete the whole link\n");
		makeEmpty(HEAD);
		return ;
	}else{
		int flags = 0;
		while((flags < position) && (HEAD->next != NULL)){
			HEAD = HEAD->next;
			flags++;
		}

		if (HEAD->next == NULL && flags < position){
			printf("the position %d is out of range,can not delete it\n",position);
			return ;
		}

		if (flags == position && HEAD->next != NULL){
			HEAD->previous->next=HEAD->next;
			HEAD->next->previous=HEAD->previous;
			free(HEAD);
			HEAD = NULL;
			return ;			
		}
		if (flags == position && HEAD->next == NULL){
			HEAD->previous->next = HEAD->next;
			free(HEAD);
			HEAD = NULL;
			return;
		}
	}
}

void listInsert(LINK_LIST *HEAD,int position,int value)
{
	//position = 0 means insert in the end of the link
	if (position < 0){
		printf("you have select a wrong position! %d\n",position);
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
	if (HEAD->next == NULL && flags < position){
		printf("the position %d is out of range of the list\n",position);
		return ;
	}

	if (flags == position && HEAD->next == NULL){
		LINK_LIST *head = nodeCreate(value);
		head->previous = HEAD;
		HEAD->next = head;
		return;
		}

	if (flags == position && HEAD->next != NULL){
		LINK_LIST *tmp;
		tmp = HEAD->next;
		HEAD->next = nodeCreate(value);
		HEAD->next->next = tmp;
		tmp->previous = HEAD->next;
		HEAD->next->previous = HEAD;
		return;
	}
}

LINK_LIST * findPosition(LINK_LIST *HEAD,const int position)
{
	int flags = 0;
	// while(flags < position && HEAD->next != NULL){
	while(flags < position && HEAD != NULL){
		flags++;
		HEAD = HEAD->next;
	}

	// if (HEAD->next == NULL){
	if (HEAD == NULL){
		printf("the position %d is out of range!\n",position);
		return (LINK_LIST *)NULL;
	}else{
		return HEAD;
	}
}


//position = 0 means print all the node. position != 0 means print the position's value
void listPrint(LINK_LIST *HEAD,const int position)
{
	if (position < 0){
		printf("position %d is not a vaild position!\n",position);
		return ;
	}

	if (position == 0){
		while(HEAD->next != NULL){
			printf("->%d",HEAD->next->value);
			HEAD = HEAD->next;
		}
		printf("\n");
		return ;
	}else{

		LINK_LIST *tmp;
		tmp = findPosition(HEAD,position);
		if (tmp == NULL){
			return ;
		}else{
			printf("the value in position %d is %d\n",position,tmp->value);
		}
	}
	return ;
}	