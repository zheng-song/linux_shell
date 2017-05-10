#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct BSTreeNode
{
	int value;
	struct BSTreeNode *left;
	struct BSTreeNode *right;
};

void LevelReverse (struct BSTreeNode *pRoot)
{
	if (pRoot == NULL){
		return;
	}
	printf("%d\n",pRoot->value);
	if (pRoot->left == NULL){
		if (pRoot->right == NULL){
			return;
		}else{
			// printf("%d\n",pRoot->right->value);
			LevelReverse(pRoot->right);
		}
	}else{
		// printf("%d\n",pRoot->left->value);
		LevelReverse(pRoot->left);
		if (pRoot->right != NULL){
			printf("%d\n",pRoot->right->value);
			LevelReverse(pRoot->right);
		}
	}
}

//create root hub and return the doc
struct BSTreeNode * BSTreeInit(int num)
{
	struct BSTreeNode *ROOT;
	ROOT=(struct BSTreeNode *)malloc(sizeof(struct BSTreeNode));
	ROOT->value = num;
	ROOT->left = NULL;
	ROOT ->right = NULL;
	return ROOT;
}

struct BSTreeNode* BSTreeCreate(struct BSTreeNode *parent,int num,int flags)
{
	if(flags == 1){
		parent->right=(struct BSTreeNode *)malloc(sizeof(struct BSTreeNode));
		parent->right->value=num;
		parent->right->left=NULL;
		parent->right->right=NULL;
		// printf("%d\n",parent->right->value);
		return ((struct BSTreeNode *)parent->right);
	}else{
		parent->left=(struct BSTreeNode *)malloc(sizeof(struct BSTreeNode));
		parent->left->value=num;
		parent->left->left=NULL;
		parent->left->right=NULL;	
		return ((struct BSTreeNode *)parent->left);
	}

}


// int main(int argc, char const *argv[])
// {
// 	struct BSTreeNode *root;
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