#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

strcut BSTreeNode
{
	int value;
	BSTreeNode *left;
	BSTreeNode *right;
};

void LevelReverse (BSTreeNode *pRoot)
{
	if (pRoot == NULL){
		return;
	}
	printf("%d\n",pRoot->vlaue);
	if (left == NULL){
		if (right = NULL){
			return;
		}else{
			printf("%d\n",right->value);
			LevelReverse(right);
		}
	}else{
		printf("%d\n",left->value);
		LevelReverse(left);
		if (right != NULL){
			printf("%d\n",right->value);
			LevelReverse(right);
		}
	}
}

//create root hub and return the doc
BSTreeNode * BSTreeInit(int num)
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
	if(flags=1){
		parent->right=(BSTreeNode *)malloc(sizeof(BSTreeNode));
		parent->right->left=NULL;
		parent->right->right=NULL;
		return right;
	}else{
		parent->left=(BSTreeNode *)malloc(sizeof(BSTreeNode));
		parent->right->left=NULL;
		parent->right->right=NULL;	
		return left;
	}

}


int main(int argc, char const *argv[])
{
	BSTreeNode *root;
	int num;
	printf("please input the head number:");
	scanf("%d",&num);
	root=BSTreeCreate(num);
	BSTreeNode *ROOT=root;
 	for (int i = 0; i < 5; ++i){
 		int number,flags;
 		printf("the number and the flags(0 for left,1 for right):");
 		scanf("%d%d",&number,&flags);
 		ROOT = BSTreeCreate(ROOT,number,flags);
 	}

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