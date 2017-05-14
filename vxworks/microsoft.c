#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct bstreeNode
{
	int value;
	struct BSTreeNode *left;
	struct BSTreeNode *right;
} BSTreeNode;

// void LevelReverse (struct BSTreeNode *pRoot)
// {
// 	if (pRoot == NULL){
// 		return;
// 	}
// 	printf("%d\n",pRoot->value);
// 	if (pRoot->left == NULL){
// 		if (pRoot->right == NULL){
// 			return;
// 		}else{
// 			// printf("%d\n",pRoot->right->value);
// 			LevelReverse(pRoot->right);
// 		}
// 	}else{
// 		// printf("%d\n",pRoot->left->value);
// 		LevelReverse(pRoot->left);
// 		if (pRoot->right != NULL){
// 			printf("%d\n",pRoot->right->value);
// 			LevelReverse(pRoot->right);
// 		}
// 	}
// }

//create root hub and return the doc
BSTreeNode *BSTreeInit(int num)
{
	BSTreeNode *ROOT;
	ROOT=(BSTreeNode *)malloc(sizeof(BSTreeNode));
	ROOT->value = num;
	ROOT->left = NULL;
	ROOT ->right = NULL;
	return ROOT;
}

BSTreeNode* BSTreeCreate(BSTreeNode *parent,int num,int flags)
{
	if(flags == 1){
		parent->right=(BSTreeNode *)malloc(sizeof(BSTreeNode));
		parent->right->value=num;
		parent->right->left=NULL;
		parent->right->right=NULL;
		// printf("%d\n",parent->right->value);
		return ((BSTreeNode *)parent->right);
	}else{
		parent->left=(BSTreeNode *)malloc(sizeof(BSTreeNode));
		parent->left->value=num;
		parent->left->left=NULL;
		parent->left->right=NULL;	
		return ((BSTreeNode *)parent->left);
	}

}


// int main(int argc, char const *argv[])
// {
// 	BSTreeNode *root;
// 	int num;
// 	printf("please input the head number:");
// 	scanf("%d",&num);
// 	root =(struct BSTreeNode *) BSTreeInit(num);
// 	printf("%d,%p,%p\n",root->value,root->left,root->left);

// 	struct BSTreeNode *ROOT_left=(struct BSTreeNode *)root;
// 	struct BSTreeNode *ROOT_right=(struct BSTreeNode *)root;
//  	for (int i = 0; i < 5; ++i){
//  		int number,flags=0;
//  		printf("the number and the flags(0 for left,1 for right):");
//  		scanf("%d%d",&number,&flags);
//  		if (flags == 0){
//  			ROOT_left = (struct BSTreeNode *)BSTreeCreate(ROOT_left,number,flags);
 		
//  		}else{
//  			ROOT_right = (struct BSTreeNode *)BSTreeCreate(ROOT_right,number,flags);
//  		}
//  	}

//  	LevelReverse(root);
// 	return 0;
// }


BSTreeNode * BSTreeInit(int a[],int num)
{
	BSTreeNode *Root=NULL;
	Root=(BSTreeNode *)malloc(sizeof(BSTreeNode));
	Root->value=a[0];
	Root->left=NULL;
	Root->right=NULL;

	BSTreeNode *parent;
	parent=(BSTreeNode *)Root;
	for (int i = 1; i <num ; ++i){	

		if (parent->value < a[1]){
			BSTreeNode *child_left;
			child_left=(BSTreeNode *)malloc(BSTreeNode);
			parent->left=(BSTreeNode *)child_left;
			parent->left->value=a[i];
			parent=parent->left;
		}else{
			BSTreeNode *child_right;
			child_right=(BSTreeNode *)malloc(BSTreeNode);
			parent->right=(BSTreeNode *)child_right;
			parent->right->value=a[i];
			parent=parent->right;
		}
	}
}

BSTreeNode * BSTreeNodeAdd(BSTreeNode *root,int a)
{
	while(root->value < a && ){
		root=root->left;
	}
}


int main(int argc, char const *argv[])
{
	int a[20]={0};
	printf("%s\n", );

	strcut BSTreeNode *head;

	return 0;
}




// strcut BSTreeNode{
// 	int m_nValue;
// 	BSTreeNode *m_pLeft;
// 	BSTreeNode *m_pRight;
// };

// void helper(BSTreeNode *&head,BSTreeNode *&tail,BSTreeNode *root)

// BSTreeNode *treeToLinkedList(BSTreeNode *root)
// {
// 	BSTreeNode *head.*tail;
// 	helper(head,tail,root);
// 	return head;
// }

// void helper(BSTreeNode *&head,BSTreeNode *&tail, BSTreeNode *root){
// 	BSTreeNode *lt,*rh;
// 	if (root == NULL){
// 		head = NULL;
// 		tail = NULL;
// 		return ;
// 	}

// 	helper(head,lt,root->m_pLeft);
// 	helper(rh,tail,root->m_pRight);
// 	if (lt!=NULL){
// 		lt->m_pRight = root;
// 		root->m_pLeft = lt;
// 	}else{
// 		head = root;
// 	}

// 	if (rh!=NULL){
// 		root->m_pRight = rh;
// 		rh->m_pLeft = root;
// 	}else{
// 		tail = root;
// 	}
// }

// int main(int argc, char const *argv[])
// {
	
// 	return 0;
// }









//==============================test.c======================================================
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


typedef struct bstreebode
{
	int value;
	struct bstreebode *left;
	struct bstreebode *right;
} BSTreeNode;

BSTreeNode *headerCreate(const int a)
{
	BSTreeNode *header = NULL;
	header = (BSTreeNode *)malloc(sizeof(BSTreeNode));
	memset(header, 0, sizeof(BSTreeNode));
	header->value = a;
	return header;
}


void TreePositionFind(BSTreeNode *header,const int a)
{
	if (header->value <= a){
		if (header->right != NULL){
			TreePositionFind(header->right, a);
		}else{
			BSTreeNode *right = NULL;
			right = (BSTreeNode *)malloc(sizeof(BSTreeNode));
			memset(right,0,sizeof(BSTreeNode));
			header->right = right;
			header->right->value = a;
		}
	}else{
		if (header->left != NULL){
			TreePositionFind(header->left,a);
		}else{
			BSTreeNode *left = NULL;
			left = (BSTreeNode *)malloc(sizeof(BSTreeNode));;
			header->left = left;
			header->left->value = a;
		}
	}
}


void cyclePrintTree(const BSTreeNode *header)
{
	static int TreeLevel = 1;
	if (header != NULL){
		printf("TreeLevel is %d,the number is%d\n",TreeLevel,header->value);
		TreeLevel++;
		cyclePrintTree(header->left);
		cyclePrintTree(header->right);
	}else{
		printf("at the end of tree\n");
	}
}


int main(int argc, char const *argv[])
{
	int num =0,flags = 0;
	BSTreeNode *header=NULL;
	printf("Create a header node,please input the header node value\n");
	scanf("%d",&num);
	header= headerCreate(num);
	printf("0 for end\t1 for continue\n");
	scanf("%d",&flags);
	while(flags){
		int nodeid;
		printf("please input node number:\t");
		scanf("%d",&nodeid);
		TreePositionFind(header,nodeid);
		printf("0 for end\t1 for continue\n");
		scanf("%d",&flags);
	}

	cyclePrintTree(header);

	return 0;
}
//==============================test.c======================================================