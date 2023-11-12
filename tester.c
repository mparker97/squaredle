#include <stdio.h>
#include <unistd.h>

char EXTENDED_CAPS[][2] = {
  // lower upper
  {129, 154}, // u umlaut
  {130, 144}, // e accent
  {132, 142}, // a umlaut
  {134, 143}, // a ring
  {135, 128}, // c flag
  {145, 146}, // ae
  {148, 153}, // o umlaut
  {164, 165}, // n tilde
};

int main(int argc, char* argv[]){
	int i;
	for (i = 0; i < sizeof(EXTENDED_CAPS) / sizeof(EXTENDED_CAPS[0]); i++){
		printf("%c %c\n", EXTENDED_CAPS[i][0], EXTENDED_CAPS[i][1]);
	}
	return 0;
}
