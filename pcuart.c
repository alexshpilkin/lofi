#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "riscv.h"
#include "rvexec.h"

/* FIXME */
#define unlikely(C) (__builtin_expect(!!(C), 0))

extern wbk_t *wbk; /* FIXME */
static unsigned char uart[8], rbr, thr;
enum { DR = 0x01, THRE = 0x20, TEMT = 0x40, TR = THRE | TEMT };
static unsigned char lsr;
static volatile sig_atomic_t rxsig, txsig;

static void sigio(int sig) {
	int e = errno;
	struct pollfd fds[2] = {0};
	fds[0].fd = STDIN_FILENO;  fds[0].events = POLLIN;
	fds[1].fd = STDOUT_FILENO; fds[1].events = POLLOUT;
	poll(&fds[0], sizeof fds / sizeof fds[0], 0);
	if (fds[0].revents & POLLIN)  rxsig = 1;
	if (fds[1].revents & POLLOUT) txsig = 1;
	errno = e;
}

static void uartwbk(struct hart *t) {
	if (lsr & TR) {
		lsr &= ~TR;
	} else if (txsig) {
		txsig = 0; write(STDOUT_FILENO, &thr, 1);
	}
	if (write(STDOUT_FILENO, &uart[0], 1) == 1)
		lsr |= TR;
	else
		thr = uart[0];
}

static unsigned char *uartmap(struct hart *t, xword_t addr, xword_t size, int type) {
	addr &= 0xFFFFF;
	if unlikely(addr >= sizeof uart || size != 1) return 0;

	switch (addr & 7) {
	case 0:
		if (uart[3] & 0x80) break;
		if (type <= MAPA) {
			if (lsr & DR) {
				uart[0] = rbr; lsr &= ~DR;
			} else if (rxsig) {
				rxsig = 0; read(STDIN_FILENO, &uart[0], 1);
			}
			if (read(STDIN_FILENO, &rbr, 1) == 1)
				lsr |= DR;
		}
		if (type >= MAPA) wbk = &uartwbk;
		break;
	case 1: uart[1] = 0; break;
	case 2: uart[2] = 0; break;
	case 4: uart[4] = 0; break;
	case 5:
		if unlikely(rxsig) {
			rxsig = 0;
			if (!(lsr & DR)) {
				read(STDIN_FILENO, &rbr, 1); lsr |= DR;
			}
		}
		if unlikely(txsig) {
			txsig = 0;
			if (!(lsr & TR)) {
				write(STDOUT_FILENO, &thr, 1); lsr |= TR;
			}
		}
		uart[5] = lsr; break;
	case 6: uart[6] = 0; break;
	}
	return &uart[addr & 7];
}

__attribute__((alias("uartmap"))) map_t meg100;

static struct termios origti;

static void restore(void) {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &origti);
}

static struct sigaction satstp;
static void sigtstp(int sig) {
	struct sigaction sa; struct termios ti;
	sigset_t tstp, mask; int e = errno;

	tcgetattr(STDIN_FILENO, &ti);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &origti);
	sigaction(sig, &satstp, &sa);

	raise(sig);
	sigemptyset(&tstp); sigaddset(&tstp, sig);
	sigprocmask(SIG_UNBLOCK, &tstp, &mask);
	/* take pending TSTP here */
	sigprocmask(SIG_SETMASK, &mask, 0);

	tcgetattr(STDIN_FILENO, &origti);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &ti);
	sigaction(sig, &sa, 0);

	errno = e;
}

static struct sigaction saquit;
static void sigquit(int sig) {
	restore(); sigaction(sig, &saquit, 0); raise(sig);
}

static struct sigaction saint;
static void sigint(int sig) {
	restore(); sigaction(sig, &saint, 0); raise(sig);
}

static struct sigaction saterm;
static void sigterm(int sig) {
	restore(); sigaction(sig, &saterm, 0); raise(sig);
}

__attribute__((constructor)) static void uartinit(void) {
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
#ifdef O_ASYNC
	sa.sa_handler = &sigio;
	sigaction(SIGIO, &sa, 0);

	if (fcntl(STDIN_FILENO, F_SETOWN, getpid()) < 0) {
		perror("fcntl"); exit(EXIT_FAILURE);
	}
	int fl = fcntl(STDIN_FILENO, F_GETFL);
	if (fl < 0 || fcntl(STDIN_FILENO, F_SETFL, fl | O_ASYNC | O_NONBLOCK) == -1) {
		perror("fcntl"); exit(EXIT_FAILURE);
	}
	if ((fcntl(STDIN_FILENO, F_GETFL) & O_ASYNC) != O_ASYNC)
#endif
		rxsig = 1;

#ifdef O_ASYNC
	if (fcntl(STDOUT_FILENO, F_SETOWN, getpid()) < 0) {
		perror("fcntl"); exit(EXIT_FAILURE);
	}
	fl = fcntl(STDOUT_FILENO, F_GETFL);
	if (fl < 0 || fcntl(STDOUT_FILENO, F_SETFL, fl | O_ASYNC | O_NONBLOCK) == -1) {
		perror("fcntl"); exit(EXIT_FAILURE);
	}
	if ((fcntl(STDOUT_FILENO, F_GETFL) & O_ASYNC) != O_ASYNC)
#endif
		txsig = 1;

	if (isatty(STDIN_FILENO) > 0) {
		struct termios ti;
		if (tcgetattr(STDIN_FILENO, &origti) < 0) {
			perror("tcgetattr"); exit(EXIT_FAILURE);
		}

#ifdef SIGTSTP
		sa.sa_handler = &sigtstp;
		if (sigaction(SIGTSTP, 0, &satstp) >= 0 && satstp.sa_handler != SIG_IGN)
			sigaction(SIGTSTP, &sa, 0);
#endif
		sa.sa_handler = &sigquit;
		if (sigaction(SIGQUIT, 0, &saquit) >= 0 && saquit.sa_handler != SIG_IGN)
			sigaction(SIGQUIT, &sa, 0);
		sa.sa_handler = &sigint;
		if (sigaction(SIGINT, 0, &saint) >= 0 && saint.sa_handler != SIG_IGN)
			sigaction(SIGINT, &sa, 0);
		/* FIXME SIGPIPE, SIGHUP */
		sa.sa_handler = &sigterm;
		sigaction(SIGTERM, &sa, &saterm);
		atexit(&restore);

		ti = origti;
		/* FIXME set raw not cbreak? */
		ti.c_iflag = ti.c_iflag & ~ICRNL;
		ti.c_lflag = ti.c_lflag & ~(ICANON | ECHO) | ISIG;
		ti.c_cc[VMIN] = 1; ti.c_cc[VTIME] = 0;
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ti) < 0) {
			perror("tcsetattr"); exit(EXIT_FAILURE);
		}
	}
}
