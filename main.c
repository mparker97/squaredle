#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>

#include "board.h"
#include "file.h"

#define PIPE_SZ 256
#define DEFAULT_TIMEOUT 60

// Attempt to read 'sz' bytes from file 'fd' into buffer 'buf'
// Fail if fewer than 'sz' bytes are read before 'timeout' seconds
int read_timeout(int fd, void *buf, size_t sz, int timeout){
	fd_set set;
	struct timeval to;
	int r;
	FD_ZERO(&set);
	FD_SET(fd, &set);
	to.tv_sec = timeout;
	r = select( fd + 1, &set, NULL, NULL, &to);
	if (r < 0){
		// TODO: error
		return r;
	}
	else if (r == 0){
		// TODO: timeout
	}
	else{
		r = read(fd, buf, sz);
		if (r < 0){
			// TODO: read failed
		}
	}
	return r;
}

#define read_timeout_default(a, b, c) read_timeout(a, b, c, DEFAULT_TIMEOUT)

int send_letter(struct word_process*, char letter){
	
}

int loop(struct word_file* wf, int p2c, int c2p){
	char buf[PIPE_SZ];
	int i;
	char c;
	for (;;){
		i = 0;
		do{
			read(c2p, buf + i, 1);
			if (buf[i] == 1){
				// TODO: quit
			}
		} while (buf[i++] != 0);
		// check word in buf		
		c = word_valid(wf, buf);
		write(p2c, &c, 1);
		if (no more words left){ // TODO
			return 1;
		}
	}
}

int start_app(struct word_file* wf){
	int f;
	int p2c[2];
	int c2p[2];
	char c[2];
	if (pipe(p2c) < 0 || pipe(c2p) < 0){
		// TODO: pipe failed
		return -1;
	}
	f = fork();
	if (f < 0){
		// TODO: fork failed
		return -1;
	}
	else if (f > 0){
		close(p2c[0]);
		close(c2p[1]);
		read_timeout_default(c2p[0], c, 1);
		if (c[0] != c2p[0]){
			// TODO: setup failure
			goto kill;
		}
		if (!loop(wf, p2c, c2p)){
			// TODO
		}

kill:
		kill(f, SIG_KILL);
		waitpid(f);
	}
	else if (f == 0){
		close(p2c[1]);
		close(c2p[0]);
		c[0] = c2p[0];
		c[1] = 0;
		exec("app.py", "app.py", c, NULL);
		printf("Failed to exec app.py\n");
	}
}

int main(int argc, char* argv[]){
	FILE* board_file;
	struct word_file wf;
	// TODO
	wf.f = fopen(word_file_name, "r");
	if (wf.f == NULL){
		// TODO: error
	}
	start_app(&wf);
	fclose(wf.f);
}
