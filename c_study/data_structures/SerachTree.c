#include <stdio.h>
#include <stdlib.h>

struct TreeNode{
	int key;
	struct TreeNode *left;
	struct TreeNode *right;
};
typedef TreeNode SearchTree;


SearchTree *createRootNode(int value)
{
	SearchTree *root;
	root = createNode(value);
	return root;
}

SearchTree *CreateNode(int value)
{
	SearchTree *node;
	node = (SearchTree *)calloc(sizeof(SearchTree));
	node->value = value;
	return node;
}

void insertNode(int value,const SearchTree const *ro ot)
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