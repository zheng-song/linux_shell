ls
#include "stdio.h"
#include "stdlib.h"

#define BUFFER_SIZE 100

int main(int argc, char const *argv[])
{
	char buf[BUFFER_SIZE];
	int charNum = 0;
	int spaceNum = 0;

	printf("please input a line:");
	scanf("%s",buf);


	while(buf[charNum] != '\0'){
		if(buf[charNum] == ' '){
			spaceNum++;
		}

		charNum++;
	}

	charNum = charNum+2 +2*spaceNum;

	char buf2[charNum];

	charNum = 0;
	int charNum2 = 0;
	while( buf[charNum] != '\0'){
		if (buf[charNum] == ' '){
			buf2[charNum2] = '%';
			buf2[++charNum2] = '2';
			buf2[++charNum2] = '0';	
		}

		buf2[charNum2] = buf[charNum];

		charNum2++;
	}

	buf2[charNum2] = '\0';


	printf("%s",buf2);

	return 0;

}