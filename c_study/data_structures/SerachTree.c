#include <stdio.h>
#include <stdlib.h>

struct TreeNode{
	int value;
	struct TreeNode *left;
	struct TreeNode *right;
};
typedef struct TreeNode SearchTree;


SearchTree *CreateNode(int value)
{
	SearchTree *node;
	node = (SearchTree *)calloc(1,sizeof(SearchTree));
	node->value = value;
	return node;
}

SearchTree *createRootNode(int value)
{
	SearchTree *root;
	root = CreateNode(value);
	return root;
}



void insertNode(int value,SearchTree *root)
{
	SearchTree *tmp = root;
	SearchTree *newNode = NULL;
	while(a <= tmp->value){
		if (tmp->left != NULL){
			tmp = tmp->left;
		}else{
			newNode = CreateNode(value);
			tmp->left = newNode;
		}
	}

}

SearchTree *createSearchTree(void)
{
	int a;
	SearchTree *root;
	root = createRootNode();	

	printf("\nplease input the number:");
	scanf("%d",&a);
	root = createRootNode(a);

	while( a!<0 ){
		printf("\nplease input the number:");
		scanf("%d",&a);
		insertNode(a,root);
	}

}

SearchTree *makeEmpty(SearchTree *root)
{

}

int main(int argc, char const *argv[])
{
	SearchTree *root;
	root = createSearchTree();

	root = makeEmpty();
	return 0;
}


SearchTree *findMax(SearchTree *root)
{

}

SearchTree *findMin(SearchTree *root)
{
	
}

void deleteElement(SearchTree *root,int value)
{
	
}