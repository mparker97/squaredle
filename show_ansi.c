#include <stdio.h>
#include <unistd.h>
#include <string.h>

// Print "SAMPLE" in each of the ANSI formatting styles

int main(int argc, char* argv[]){
	int i;
	for (i = 0; i < 108; i++){
		printf("%d: \e[%dmSAMPLE\e[0m\n", i, i);
	}
	return 0;
}
