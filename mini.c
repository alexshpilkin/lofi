#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "riscv.h"
#include "rvexec.h"

#define IMGSIZ (XWORD_C(64) * 1024 * 1024)
#define BASE XWORD_C(0x80000000)

#ifdef DEFINE_INTERRUPT
DEFINE_INTERRUPT(MTI)
#endif

/* FIXME */
static unsigned char image[IMGSIZ], uart[8], msip[4], mtime[8], mtimecmp[8];
static uint_least64_t time_, timecmp, timeorg;

static timer_t timer;
static volatile sig_atomic_t fired;

/* FIXME */
#define unlikely(C) (__builtin_expect(!!(C), 0))

wbk_t *wbk; /* FIXME move to hart */

static void uartwbk(struct hart *t) {
	if (!(uart[3] & 0x80) && write(STDOUT_FILENO, &uart[0], 1) < 0) abort();
}

static unsigned char *uartmap(struct hart *t, xword_t addr, xword_t size, int type) {
	addr &= 0xFFFFF;
	if unlikely(addr >= sizeof uart || size != 1) return 0;

	switch (addr & 7) {
	case 0:
		if (type <= MAPA && !(uart[3] & 0x80))
			(void)read(STDIN_FILENO, &uart[0], 1);
		if (type >= MAPA) wbk = &uartwbk;
		break;
	case 1: uart[1] = 0; break;
	case 2: uart[2] = 0; break;
	case 4: uart[4] = 0; break;
	case 5: { /* FIXME EOF on input? */
		struct pollfd fds[2] = {0};
		fds[0].fd = STDIN_FILENO;  fds[0].events = POLLIN;
		fds[1].fd = STDOUT_FILENO; fds[1].events = POLLOUT;
		poll(&fds[0], sizeof fds / sizeof fds[0], 0);
		uart[5] = (fds[0].revents & POLLIN  ? 0x01 : 0) |
		          (fds[1].revents & POLLOUT ? 0x60 : 0);
		break; }
	case 6: uart[6] = 0; break;
	}
	return &uart[addr & 7];
}

__attribute__((alias("uartmap"))) map_t meg100;

static void sigalrm(int sig) {
	(void)sig; fired = 1;
}

static void mtimecmpwbk(struct hart *t) {
	struct mhart *m = (struct mhart *)t;

	timecmp =         mtimecmp[0]       |
	  (uint_least64_t)mtimecmp[1] <<  8 |
	  (uint_least64_t)mtimecmp[2] << 16 |
	  (uint_least64_t)mtimecmp[3] << 24 |
	  (uint_least64_t)mtimecmp[4] << 32 |
	  (uint_least64_t)mtimecmp[5] << 40 |
	  (uint_least64_t)mtimecmp[6] << 48 |
	  (uint_least64_t)mtimecmp[7] << 56;

	struct itimerspec it = {0};
	if (time_ >= timecmp) {
		timer_settime(timer, 0, &it, 0); /* disarm */
		m->pending |= XWORD_C(1) << MTI;
	} else {
		uint_least64_t deadline = timecmp - timeorg;
		it.it_value.tv_sec  = deadline / 1000000;
		it.it_value.tv_nsec = deadline % 1000000 * 1000;
		timer_settime(timer, TIMER_ABSTIME, &it, 0);
		m->pending &= ~(XWORD_C(1) << MTI);
	}
}

static void mtimewbk(struct hart *t) {
	uint_least64_t tm = time_ - timeorg;
	time_ =           mtime[0]       |
	  (uint_least64_t)mtime[1] <<  8 |
	  (uint_least64_t)mtime[2] << 16 |
	  (uint_least64_t)mtime[3] << 24 |
	  (uint_least64_t)mtime[4] << 32 |
	  (uint_least64_t)mtime[5] << 40 |
	  (uint_least64_t)mtime[6] << 48 |
	  (uint_least64_t)mtime[7] << 56;
	timeorg = time_ - tm;
	mtimecmpwbk(t);
}

static void msipwbk(struct hart *t) {
	xword_t v = msip[0]        |
	  ((xword_t)msip[1] << 8)  |
	  ((xword_t)msip[2] << 16) |
	  ((xword_t)msip[3] << 24);
	if (v) abort();
}

static unsigned char *clintmap(struct hart *t, xword_t addr, xword_t size, int type) {
	addr &= 0xFFFFF;
	if (addr - 0x04000 < sizeof mtimecmp && size == 4) {
		mtimecmp[7] = timecmp >> 56 & 0xFF;
		mtimecmp[6] = timecmp >> 48 & 0xFF;
		mtimecmp[5] = timecmp >> 40 & 0xFF;
		mtimecmp[4] = timecmp >> 32 & 0xFF;
		mtimecmp[3] = timecmp >> 24 & 0xFF;
		mtimecmp[2] = timecmp >> 16 & 0xFF;
		mtimecmp[1] = timecmp >>  8 & 0xFF;
		mtimecmp[0] = timecmp       & 0xFF;
		if (type >= MAPA) wbk = &mtimecmpwbk;
		return &mtimecmp[addr & 4];
	}
	if (addr - 0x0BFF8 < sizeof mtime && size == 4) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		time_ = timeorg +
		        (uint_least64_t)ts.tv_sec * 1000000 +
		        ts.tv_nsec / 1000;

		mtime[7] = time_ >> 56 & 0xFF;
		mtime[6] = time_ >> 48 & 0xFF;
		mtime[5] = time_ >> 40 & 0xFF;
		mtime[4] = time_ >> 32 & 0xFF;
		mtime[3] = time_ >> 24 & 0xFF;
		mtime[2] = time_ >> 16 & 0xFF;
		mtime[1] = time_ >>  8 & 0xFF;
		mtime[0] = time_       & 0xFF;
		if (type >= MAPA) wbk = &mtimewbk;
		return &mtime[addr & 4];
	}
	if (addr == 0x00000 && size == 4) {
		wbk = &msipwbk; return &msip[0];
	}
	return 0;
}

__attribute__((alias("clintmap"))) map_t meg110;

static unsigned char *imagemap(struct hart *t, xword_t addr, xword_t size, int type) {
	addr -= BASE;
	if unlikely(addr >= sizeof image || size > sizeof image - addr)
		return 0;
	return &image[addr];
}

#define meg100 meg100_ /* defined here */
#define meg110 meg110_ /* defined here */
__attribute__((alias("imagemap"), weak)) map_t HEX12(meg); /* FIXME */
#undef meg100
#undef meg110

unsigned char *map(struct hart *t, xword_t addr, xword_t size, int type) {
	static map_t *const meg[0x1000] = { HEX12(&meg) };
	if unlikely(addr & size - 1) switch (type) {
	case MAPR: trap(t, RALIGN, addr); return 0;
	case MAPX: break; /* handled by higher-level code */
	case MAPA: case MAPW: trap(t, WALIGN, addr); return 0;
	}
	unsigned char *p = (*meg[addr >> 20])(t, addr, size, type);
	if unlikely(!p) switch (type) {
	case MAPR: trap(t, RACCES, addr); return 0;
	case MAPX: trap(t, XACCES, addr); return 0;
	case MAPA: case MAPW: trap(t, WACCES, addr); return 0;
	}
	return p;
}

void unmap(struct hart *t) {
	struct mhart *restrict m = (struct mhart *)t;

	if unlikely(wbk) {
		(*wbk)(t); wbk = 0;
	}
	if unlikely(fired) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		time_ = timeorg +
		        (uint_least64_t)ts.tv_sec * 1000000 +
		        ts.tv_nsec / 1000;
		if (time_ >= timecmp) {
			m->pending |= XWORD_C(1) << MTI; fired = 0;
		}
	}
}

xword_t xalign(struct hart *t, xword_t pc) {
	(void)t; return pc & ~XWORD_C(3);
}

static size_t readfile(const char *name, unsigned char *p) {
	FILE *fp; size_t n = image + sizeof image - p;
	if (!(fp = fopen(name, "rb"))) {
		perror(name); exit(EXIT_FAILURE);
	}
	n = fread(p, 1, n, fp);
	if (ferror(fp)) {
		perror(name); exit(EXIT_FAILURE);
	}
	if (!feof(fp)) {
		fprintf(stderr, "%s: File too big", name); exit(EXIT_FAILURE);
	}
	fclose(fp);
	return n;
}

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

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "usage: %s IMAGE DTB\n", argv[0]);
		return EXIT_FAILURE;
	}

	size_t dtb = readfile(argv[1], &image[0]) + 3 & ~(size_t)3;
	readfile(argv[2], &image[dtb]);

	int fl = fcntl(STDIN_FILENO, F_GETFL);
	if (fl < 0 || fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK) == -1) {
		perror("stdin"); return EXIT_FAILURE;
	}

	if (isatty(STDIN_FILENO) > 0) {
		struct termios ti;
		if (tcgetattr(STDIN_FILENO, &origti) < 0) {
			perror("tcgetattr"); return EXIT_FAILURE;
		}

		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
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
			perror("tcsetattr"); return EXIT_FAILURE;
		}
	}

	struct sigaction sa = {0};
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = &sigalrm;
	sigaction(SIGALRM, &sa, 0);

#if defined _XOPEN_REALTIME && _XOPEN_REALTIME + 0 >= 0 && \
    defined _POSIX_MONOTONIC_CLOCK && _POSIX_MONOTONIC_CLOCK + 0 >= 0
/* implies _POSIX_TIMERS */
	struct sigevent se = {0};
	se.sigev_notify = SIGEV_SIGNAL;
	se.sigev_signo  = SIGALRM;
	if (timer_create(CLOCK_MONOTONIC, &se, &timer) < 0) {
		perror("timer_create"); return EXIT_FAILURE;
	}
#else
#error "no timer_create or CLOCK_MONOTONIC?"
#endif

	struct mhart mhart = {0};
	mhart.hart.pc = BASE;
	mhart.hart.ireg[11] = BASE + dtb;

	for (;;) {
		xword_t pc = mhart.hart.pc;
		const unsigned char *restrict ip = map(&mhart.hart, pc, 4, MAPX);
		if (ip) {
			uint_least32_t i = ip[0]       |
			   (uint_least16_t)ip[1] <<  8 |
			   (uint_least32_t)ip[2] << 16 |
			   (uint_least32_t)ip[3] << 24;
			xword_t nextpc = mhart.hart.nextpc = pc + 4 & XWORD_MAX;

			execute(&mhart.hart, i);
			if unlikely(mhart.hart.nextpc & 3)
				trap(&mhart.hart, XALIGN, mhart.hart.nextpc);
			else if (mhart.hart.lr)
				mhart.hart.ireg[mhart.hart.lr] = nextpc;
		}
		unmap(&mhart.hart);
		mhart.hart.lr = 0;
		mhart.hart.pc = mhart.hart.nextpc;
		if (unlikely(mhart.mie & mhart.pending) && (mhart.mstatus & MSTATUS_MIE || mhart.pending & UMODE)) {
			trap(&mhart.hart, INTRPT + MTI, 0);
			mhart.hart.pc = mhart.hart.nextpc;
		}
	}
}
