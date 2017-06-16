#include <stdio.h>
#include <stdlib.h>

struct TreeNode{
	int value;
	struct TreeNode *left;
	struct TreeNode *right;
};
typedef struct TreeNode SearchTree;


void insertNode(int value,SearchTree *root);

/*输入要创建的二叉查找树节点的值(假定全为正数),
 *创建该二叉查找树,以-1结束输入.返回根节点指针.
 */
SearchTree *createSearchTree(void);

/*清空创建的二叉树,释放内存*/ 	
void makeEmpty(SearchTree *root);

void pre_Order(SearchTree *root);  	//先根遍历
void mid_Order(SearchTree *root);	//中根遍历
void post_Order(SearchTree *root);	//后根遍历
void layer_Order(SearchTree *root);	//层次遍历
void mid_Order_record(SearchTree *root);

/*将二叉树转换为有序双向链表,左孩子指向前一个节点,右孩子指向后一个节点*/
SearchTree * convertToSortLink(SearchTree * root);

int *p = NULL;
static int i = 0;


int main(int argc, char const *argv[])
{
	SearchTree *root;
	
	root = createSearchTree();
	
	printf("pre_Order:");
	pre_Order(root);
	printf("\n");
	
	printf("mid_Order:");
	mid_Order(root);
	printf("\n");

	printf("post_Order:");
	post_Order(root);
	printf("\n");

	printf("layer_Order:");
	layer_Order(root);
	printf("\n");

	p = (int *)malloc(20*sizeof(int *));
	printf("P:%p\n",p);

	mid_Order_record(root);	
	// convertToSortLink(root);

	printf("\n%d\n",*p);

	int j=0;
	int *tmp = p;
	while(((SearchTree*)*tmp) != NULL){
		printf("%d->",((SearchTree*)(*tmp+j))->value);
		j++;
	}


	printf("makeEmpty:");
	makeEmpty(root);
	return 0;
}

void save_pointer(SearchTree *tmp)
{	
	int *t = p;
	t = (t+i);
	printf("1.%p\n",t);
	*t = (int *)tmp;
	i++;
	return ;
}

void mid_Order_record(SearchTree *root)
{
	SearchTree *tmp = root;
	if(tmp != NULL){
		mid_Order_record(tmp->left);
		printf("%p->",tmp);
		save_pointer(tmp);
		mid_Order_record(tmp->right);
		// printf("%d->",tmp->value);
	}
	return ;
}


/*SearchTree * convertToSortLink(SearchTree * root)
{
	mid_Order_record(root);

	int j = 0;
	// int *tmp = p;
	printf("pointer value:");
	while( j < 10  ){
		printf("%d->",((SearchTree *)(*(p+j)))->value);
		j++;
	}

	return (SearchTree *)p;
	
}
*/

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

SearchTree *createSearchTree(void)
{
	int a;
	SearchTree *root;
	printf("\nplease input the number:");
	scanf("%d",&a);
	root = createRootNode(a);

	printf("please input the number:");
	scanf("%d",&a);
	while(a >= 0){
		insertNode(a,root);
		printf("please input the number:");
		scanf("%d",&a);
	}
	return root;
}

void insertNode(int value,SearchTree *root)
{
	SearchTree *tmp = root;
	SearchTree *newNode = NULL;
	int flags =1;
	while(flags == 1){
		if(value <=tmp->value){
			if(tmp->left != NULL){
				tmp = tmp->left;
			}else{
				newNode = CreateNode(value);
				tmp->left = newNode;
				flags = 0;
			}
		}else{
			if(tmp->right != NULL){
				tmp = tmp->right;
			}else{
				newNode = CreateNode(value);
				tmp->right = newNode;
				flags = 0;
			}
		}
	}
	return ;
	
}


void pre_Order(SearchTree *root)
{
	SearchTree *tmp = root;
	if (tmp != NULL){
		printf("%d->",tmp->value);
		pre_Order(tmp->left);
		pre_Order(tmp->right);
	}
	return ;
}

void mid_Order(SearchTree *root)
{
	SearchTree *tmp = root;
	if(tmp != NULL){
		mid_Order(tmp->left);
		printf("%d->",tmp->value);
		printf("%p->",tmp);
		mid_Order(tmp->right);
	}
	return ;
}

void post_Order(SearchTree *root)
{
	SearchTree *tmp = root;
	if(tmp != NULL){
		post_Order(tmp->left);
		post_Order(tmp->right);
		printf("%d->",tmp->value);
	}
	return ;
}

void layer_Order(SearchTree *root)
{
	SearchTree * tmp = root;
	SearchTree *p[100] = {0};
	int i = 0,j=0;

	p[i] = tmp;
	// printf("1.%d\n",p[j]->value);
	while(p[j] != NULL){
		printf("%d->",p[j]->value);
		if(p[j]->left != NULL){
			i++;
			p[i] = p[j]->left;
		}
		
		if(p[j]->right != NULL){
			i++;
			p[i] =p[j]->right;
		}

		j++;
	}
	printf("\n");
	return ;
}

void makeEmpty(SearchTree *root)
{
	if (root != NULL){
		SearchTree * tmpL = root->left;
		SearchTree * tmpR = root->right;
		free(root);
		root = NULL;
		makeEmpty(tmpL);
		makeEmpty(tmpR);
	}

	return ;
}

