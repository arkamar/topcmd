#include <sys/ioctl.h>

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#define LEN(x) ( sizeof x / sizeof * x )

struct winsize ws;

static
void
do_header() {
	time_t t = time(NULL);
	char * ts = ctime(&t);
	puts(ts);
}

static
int
read_pipe(FILE * p) {
	char * line = NULL;
	size_t cap = 0;
	ssize_t len;

	while ((len = getline(&line, &cap, p)) > 0) {
		fputs(line, stdout);
	}

	free(line);

	fflush(stdout);

	return 0;
}

static
int
read_pipe_fd(const int fd) {
	FILE * p;
	int ret;

	p = fdopen(fd, "r");
	if (p == NULL) {
		perror("fdopen");
		return -1;
	}

	ret = read_pipe(p);

	fclose(p);

	return ret;
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

	return read_pipe_fd(pfd[0]);
}

int
main(int argc, char * argv[]) {
	const char * argv0 = *argv; argv++; argc--;

	/* get window size of the terminal */
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
		perror("ioctl");
		return 1;
	}

	struct pollfd pfd[] = {
		{ STDIN_FILENO, POLLIN, 0 },
	};

	fputs("\033[2J", stdout); /* clear entire screen */

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

			fputs("\033[2J", stdout); /* clear entire screen */
			fputs("\033[H", stdout); /* set cursor to 0,0 position */

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
