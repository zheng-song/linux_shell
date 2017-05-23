#include <stdio.h>
#include <stdlib.h>

struct search_tree_node
{
	int value;
	struct search_tree_node *left;
	struct search_tree_node *right;
};

typedef struct search_tree_node NODE;
static NODE *rootNode = NULL;


static NODE *nodeCreate(int value)
{
	NODE *node;
	node = (NODE *)calloc(1,sizeof(NODE));
	node->value = value;
	return node;
}

static void insertNode(const NODE *node,const NODE *rootNode)
{
	NODE *parentNode = rootNode;

	while(node->value > parentNode->value && parentNode->right != NULL){
		parentNode = parentNode->right;
		
		while(node->value < parentNode->value && parentNode->left != NULL){
			parentNode = parentNode->left;
		}						
	}

	while(node->value < parentNode->value && parentNode->left != NULL){
		parentNode = parentNode->left
		while(node->value > parentNode->value && parentNode->right != NULL){
			parentNode = parentNode->right;
		}
	}


	if(node->value > parentNode->value && parentNode ->right == NULL){
		parentNode->right = node;
		return ;
	}else if (node->value <= parentNode->value && parentNode -> left == NULL){
		parentNode->left = node;
		return ;
	}
}


static NODE *treeCreate(void)
{
	char status ='y'
	int value;
	NODE *node;
	printf("\nenter the root node value:");
	scanf("%d\n",&value);
	rootNode = nodeCreate(value);
	
	printf("\ndo you want cantinue? 'Y' or 'N':");
	status = getchar();
	while(status == 'y' || status == 'Y'){
		printf("enter tne node value:\n");
		scanf("%d",&value);
		node = nodeCreate(value);
		insertNode(node,rootNode);
		printf("\ndo you want cantinue? 'Y' or 'N':");
		scanf("%c",&status);
	}

}

void printSubTree(const NODE *node)
{
	printf("%d->",node->value);
	return ;
}

static void printTree(const NODE *rootNode)
{
	NODE *leftTmp = rootNode;
	NODE *rightTmp = rootNode;

	printf("%d->",rootNode->value);
	while(leftTmp->left != NULL){
		printSubTree(leftTmp->left);
		leftTmp = leftTmp->left;
	}

	while(rightTmp->right != NULL){
		printSubTree(rightTmp->right);
		rightTmp =rightTmp->right;
	}
	return ;
}


int main(int argc, char const *argv[])
{
	treeCreate();
	printTree(rootNode);

	return 0;
}
