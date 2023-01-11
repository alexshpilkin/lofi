#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
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
static unsigned char image[IMGSIZ], msip[4], mtime[8], mtimecmp[8];
static uint_least64_t time_, timecmp, timeorg;

static timer_t timer;
static volatile sig_atomic_t fired;

/* FIXME */
#define unlikely(C) (__builtin_expect(!!(C), 0))

wbk_t *wbk; /* FIXME move to hart */

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

#define meg110 meg110_ /* defined here */
__attribute__((alias("imagemap"), weak)) map_t HEX12(meg); /* FIXME */
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

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "usage: %s IMAGE DTB\n", argv[0]);
		return EXIT_FAILURE;
	}

	size_t dtb = readfile(argv[1], &image[0]) + 3 & ~(size_t)3;
	readfile(argv[2], &image[dtb]);

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
