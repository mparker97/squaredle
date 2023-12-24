#include <stdio.h>
#include <unistd.h>
#include <string.h>

struct b{
	int g;
	int h;
};

struct a{
	int* p;
	union{
		struct{
			int g;
			int h;
		};
		struct b q;
	};
	int* c;
};

int main(int argc, char* argv[]){
	struct a test;
	test.g = 1;
	printf("%d\n", test.g);
	return 0;
}
