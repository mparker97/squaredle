#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>

int main(int argc, char* argv[]){
	int i;
	wchar_t block = 0x2588;
	setlocale(LC_CTYPE, "");
	for (i = 0; i < 8; i++){
		wprintf(L"%lc\n", block + i);
	}
	return 0;
}
