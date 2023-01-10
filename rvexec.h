/* requires stdint.h, riscv.h */

typedef void execute_t(struct hart *, uint_least32_t);

#define HEX3(T) \
	T##0, T##1, T##2, T##3, T##4, T##5, T##6, T##7

execute_t HEX3(memo), HEX3(amop);

#define HEX4(T) \
	T##0, T##1, T##2, T##3, T##4, T##5, T##6, T##7, \
	T##8, T##9, T##A, T##B, T##C, T##D, T##E, T##F

#define HEX5(T) \
	HEX4(T##0), HEX4(T##1)

execute_t HEX5(exec);

#define HEX7(T) \
	HEX4(T##0), HEX4(T##1), HEX4(T##2), HEX4(T##3), \
	HEX4(T##4), HEX4(T##5), HEX4(T##6), HEX4(T##7)

execute_t HEX7(xops), HEX7(wops);

#define HEX8(T) \
	HEX4(T##0), HEX4(T##1), HEX4(T##2), HEX4(T##3), \
	HEX4(T##4), HEX4(T##5), HEX4(T##6), HEX4(T##7), \
	HEX4(T##8), HEX4(T##9), HEX4(T##A), HEX4(T##B), \
	HEX4(T##C), HEX4(T##D), HEX4(T##E), HEX4(T##F)

#define HEX12(T) \
	HEX8(T##0), HEX8(T##1), HEX8(T##2), HEX8(T##3), \
	HEX8(T##4), HEX8(T##5), HEX8(T##6), HEX8(T##7), \
	HEX8(T##8), HEX8(T##9), HEX8(T##A), HEX8(T##B), \
	HEX8(T##C), HEX8(T##D), HEX8(T##E), HEX8(T##F)

#ifdef __GNUC__
#define DEFINE_EXTENSION(C) \
	__asm__ ( ".pushsection .comment.misa, \"\", @nobits\n\t" \
	          ".skip 1 << ('" #C "' - 'A')\n\t" \
	          ".popsection" \
	        );
#define DEFINE_INTERRUPT(N) DEFINE_INTERRUPT_(N)
#define DEFINE_INTERRUPT_(N) \
	__asm__ ( ".pushsection .comment.mies, \"\", @nobits\n\t" \
	          ".skip 1 << " #N "\n\t" \
	          ".popsection" \
	        );
#endif
