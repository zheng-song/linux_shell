#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#define strLen sizeof(LINK_LIST)
struct node
{
	int value;
	struct node *previous;
	struct node *next;
};

typedef struct node LINK_LIST;

// typedef struct linklist
// {
// 	int value;
// 	struct linklist *previous;
// 	struct linklist *next;
// } LINK_LIST;



