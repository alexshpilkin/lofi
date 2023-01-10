/* requires stdint.h, riscv.h */

typedef void execute_t(struct hart *, uint_least32_t);

#define FUNCT3(T) \
	T##0, T##1, T##2, T##3, T##4, T##5, T##6, T##7

execute_t FUNCT3(memo), FUNCT3(amop);

#define HEX4(T) \
	T##0, T##1, T##2, T##3, T##4, T##5, T##6, T##7, \
	T##8, T##9, T##A, T##B, T##C, T##D, T##E, T##F

#define OPCODE(T) \
	HEX4(T##0), HEX4(T##1)

execute_t OPCODE(exec);

#define FUNCT7(T) \
	HEX4(T##0), HEX4(T##1), HEX4(T##2), HEX4(T##3), \
	HEX4(T##4), HEX4(T##5), HEX4(T##6), HEX4(T##7)

execute_t FUNCT7(xops), FUNCT7(wops);

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
