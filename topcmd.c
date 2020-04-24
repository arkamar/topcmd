#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#define LEN(x) ( sizeof x / sizeof * x )

static
void
do_header() {
	time_t t = time(NULL);
	char * ts = ctime(&t);
	puts(ts);
}

static
int
run_cmd(char * const argv[]) {
	int pfd[2];

	if (pipe(pfd) == -1) {
		perror("pipe");
		return -1;
	}

	int pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid == 0) { /* child */
		close(pfd[0]);
		dup2(pfd[1], STDOUT_FILENO);
		dup2(pfd[1], STDERR_FILENO);
		if (pfd[1] != STDOUT_FILENO) {
			close(pfd[1]);
		}
		if (*argv) {
			execvp(argv[0], argv);
			perror("execvp");
		}
		_exit(127);
	} /* end child */

	/* parent continues here */

	close(pfd[1]);
	FILE * p = fdopen(pfd[0], "r");
	if (p == NULL) {
		perror("fdopen");
		exit(1);
	}

	for (int i = 0 ; i < 200; i++) {
		wint_t c = fgetwc(p);
		//addnwstr((wchar_t *)&c, 1);
	}
	fclose(p);

	return 0;
}

int
main(int argc, char * argv[]) {
	const char * argv0 = *argv; argv++; argc--;

	struct pollfd pfd[] = {
		{ STDIN_FILENO, POLLIN, 0 },
	};

	for (;;) {
		char buf[BUFSIZ];
		ssize_t len;
		int ret;

		ret = poll(pfd, LEN(pfd), -1);

		if (ret == -1) {
			perror("poll");
			break;
		}

		if (pfd[0].revents & POLLIN) {
			len = read(STDIN_FILENO, buf, sizeof buf);
			if (len == -1) {
				perror("read");
				break;
			} else if (len == 0) {
				break;
			}

			do_header();
			if (run_cmd(argv)) {
				break;
			}

		}

		if (pfd[0].revents & POLLHUP) {
			break;
		}

	}

	return 0;
}
