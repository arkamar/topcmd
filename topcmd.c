#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#define LEN(x) ( sizeof x / sizeof * x )

int
main(int argc, char * argv[]) {
	const char * argv0 = *argv; argv++; argc--;

	struct pollfd pfd[] = {
		{ STDIN_FILENO, POLLIN, 0 },
	};

	for (;;) {
		char buf[BUFSIZ];
		ssize_t len;
		int p[2];
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

			if (pipe(p) == -1) {
				perror("pipe");
				break;
			}

			int pid = fork();
			if (pid == -1) {
				perror("fork");
				break;
			} else if (pid == 0) { /* child */
				close(p[0]);
				dup2(p[1], STDOUT_FILENO);
				dup2(p[1], STDERR_FILENO);
				if (p[1] != STDOUT_FILENO) {
					close(p[1]);
				}
				if (argc >= 1) {
					execvp(argv[0], argv);
					perror("execvp");
				}
				_exit(127);
			} /* end child */

			close(p[0]);
			close(p[1]);

		}

		if (pfd[0].revents & POLLHUP) {
			break;
		}

	}

	return 0;
}
