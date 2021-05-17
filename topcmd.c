#define _GNU_SOURCE
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <sys/wait.h>

#include <errno.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#define LEN(x) ( sizeof x / sizeof * x )
#define TAB_WIDTH 8

struct winsize ws;

static
void
do_header() {
	time_t t = time(NULL);
	char * ts = ctime(&t);
	fwprintf(stdout, L"%s", ts);
}

static
void
process_ansi(FILE * p) {
	wint_t c = fgetwc(p);

	if (c != L'[') {
		ungetwc(c, p);
		return;
	}

	fputwc(c, stdout);

	for (;;) {
		c = fgetwc(p);
		fputwc(c, stdout);
		if (c == L'm') {
			break;
		}
	}
}

static
void
cursor_next_line(FILE * stream) {
	/* CSI n E  CNL   Cursor Next Line
	 * Moves cursor to beginning of the line n (default 1) lines down.  */
	fputws(L"\033[E", stream);
}

static
int
read_pipe(FILE * p) {
	int x, y, w;

	const int col = ws.ws_col;
	const int row = ws.ws_row;

	fputws(L"\033[3H", stdout); /* set cursor to 2,0 position */

	for (x = 0, y = 2; x <= col && y < row;) {
		int c = fgetwc(p);
		switch (c) {
		case L'\033':
			fputwc(c, stdout);
			process_ansi(p);
			break;
		case L'\n':
			y++;
			x = 0;
			if (y >= row) {
				continue;
			}
			cursor_next_line(stdout);
			break;
		case L'\t':
			x += TAB_WIDTH - (x % TAB_WIDTH);
			if (x > col) {
				y++;
				x = TAB_WIDTH;
				if (y >= row) {
					continue;
				}
				cursor_next_line(stdout);
			}
			fputwc(c, stdout);
			break;
		case WEOF:
			if (ferror(p)) {
				perror("fgetwc");
			}
			goto exit;
		default:
			w = wcwidth(c);
			x += w;
			if (x > col) {
				y++;
				x = w;
				if (y >= row) {
					continue;
				}
				cursor_next_line(stdout);
			}
			fputwc(c, stdout);
			break;
		}
	}

exit:
	fputws(L"\033[0m", stdout);

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

static
void
sigchld(int unused) {
	int stat;
	int pid;
	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0);
}

static
int
prepare_signal_fd() {
	sigset_t mask;
	int fd, ret;

	signal(SIGCHLD, sigchld);
	sigemptyset(&mask);
	sigaddset(&mask, SIGWINCH);

	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (ret == -1) {
		perror("sigprocmask");
		return -1;
	}

	fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (ret == -1) {
		perror("signalfd");
		return -1;
	}

	return fd;
}

static
int
dump(const int fd) {
	char buf[BUFSIZ];
	ssize_t len;
	len = read(fd, buf, sizeof buf);
	if (len == -1) {
		perror("read");
	}
	return len;
}

static
long
timespec_to_ms(const struct timespec * ts) {
	return ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
}

static
int
doit(const int fd, char * argv[]) {
	struct timespec ts, te;
	int ret;

	dump(fd);

	fputws(L"\033[2J", stdout); /* clear entire screen */
	fputws(L"\033[H", stdout); /* set cursor to 0,0 position */

	do_header();

	clock_gettime(CLOCK_REALTIME, &ts);
	ret = run_cmd(argv);
	clock_gettime(CLOCK_REALTIME, &te);

	fwprintf(stdout, L"\033[1;30H%ldms", timespec_to_ms(&te) - timespec_to_ms(&ts));

	fflush(stdout);

	return ret;
}

int
main(int argc, char * argv[]) {
	const char * argv0;
	int sfd;

	argv0 = *argv; argv++; argc--;
	(void)argv0;

	setlocale(LC_CTYPE, "");

	/* set stdout to wide-character mode */
	fwide(stdout, 1);

	/* get window size of the terminal */
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
		perror("ioctl");
		return 1;
	}

	sfd = prepare_signal_fd();

	enum { POLL_STDIN, POLL_SIGNAL };
	struct pollfd pfd[] = {
		{ STDIN_FILENO, POLLIN, 0 },
		{ sfd, POLLIN, 0 },
	};

	setvbuf(stdout, NULL, _IOFBF, BUFSIZ);

	fputws(L"\033[2J", stdout); /* clear entire screen */

	for (int ret = 0; !ret;) {

		ret = poll(pfd, LEN(pfd), -1);

		if (ret == -1) {
			if (errno == EINTR) {
				ret = 0;
				continue;
			}
			perror("poll");
			break;
		}

		if (pfd[POLL_SIGNAL].revents & POLLIN) {
			/* get window size of the terminal */
			if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
				perror("ioctl");
				return 1;
			}

			ret = doit(pfd[POLL_SIGNAL].fd, argv);
		}
		if (pfd[POLL_STDIN].revents & POLLIN) {
			ret = doit(pfd[POLL_STDIN].fd, argv);
		}

		if (pfd[POLL_STDIN].revents & POLLHUP || pfd[POLL_SIGNAL].revents & POLLHUP) {
			break;
		}

	}

	return 0;
}
