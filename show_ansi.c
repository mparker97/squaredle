#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char* argv[]){
	int i;
	for (i = 0; i < 108; i++){
		printf("%d: \e[%dmSAMPLE\e[0m\n", i, i);
	}
	printf("\xe2\x80\xa6\n");
	return 0;
}
