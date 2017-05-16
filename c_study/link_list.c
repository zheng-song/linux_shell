#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define strLen sizeof(LINK_LIST)

typedef struct linklist
{
	int value;
	struct linklist *previous;
	struct linklist *next;
} LINK_LIST;

static LINK_LIST *headCreate(void)
{
	LINK_LIST *HEAD = (LINK_LIST *)malloc(strLen);
	memset(HEAD,0,strLen);
	return HEAD;
}



static void listInsert(LINK_LIST *HEAD,int position,int value)
{
	//position = 0 means insert in the end of the link
	if (position < 0){
		printf("you have select a wrong position!\n");
		return;
	}
	if (position == 0){
		if (HEAD->next != NULL){
			listInsert(HEAD->next,0,value);
		}else{
			HEAD->next = headCreate();
			HEAD->next->value = value;
			HEAD->next->next = NULL;
			HEAD->next->previous = HEAD;
		}
	}else{
		static int flags = 0;
		if (flags < position){
			flags++;
			if (HEAD->next != NULL)
			{
				listInsert(HEAD->next,position,value);
			}else{
				printf("the position %d is out of range of the list\n",position);
			}
		}else{
			LINK_LIST *tmp;
			tmp = HEAD->next;
			HEAD->next = headCreate();
			HEAD->next->value = value;
			HEAD->next->next = tmp;
			tmp->previous = HEAD->next;
			HEAD->next->previous = HEAD;
		}
	}
}


static void listPrint(LINK_LIST *HEAD,const int position)
{
	//position = 0 means print all the node 
	if (position == 0){
		if(HEAD->next != NULL){
			printf("->%d",HEAD->next->value);
			listPrint(HEAD->next,0);
		}else{
			printf("\n");
			return ;	
		}
	}else{
// position != 0 means print the position's value
		printf("find the position\n");
	}
	return ;
}


static void sortList(LINK_LIST *HEAD)
{
	
}



int main(int argc, char const *argv[])
{
	LINK_LIST *head = headCreate();
	printf("head value is %d\n",head->value);

	listInsert(head,0,12);
	listInsert(head,0,11);
	listInsert(head,0,5);
	listInsert(head,0,21);
	listInsert(head,9,19);

	printf("head");
	listPrint(head,0);



	return 0;
}