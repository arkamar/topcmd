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
		}

		if (pfd[0].revents & POLLHUP) {
			break;
		}

	}

	return 0;
}
