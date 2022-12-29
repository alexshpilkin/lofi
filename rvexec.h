/* requires stdint.h, riscv.h */

typedef void execute_t(struct hart *, uint_least32_t);

#define OPCODE(T) \
	T##00, T##01, T##02, T##03, T##04, T##05, T##06, T##07, \
	T##08, T##09, T##0A, T##0B, T##0C, T##0D, T##0E, T##0F, \
	T##10, T##11, T##12, T##13, T##14, T##15, T##16, T##17, \
	T##18, T##19, T##1A, T##1B, T##1C, T##1D, T##1E, T##1F

execute_t OPCODE(exec);

#define FUNCT3(T) \
	T##0, T##1, T##2, T##3, T##4, T##5, T##6, T##7

execute_t FUNCT3(memo);

#define FUNCT7(T) \
	T##00, T##01, T##02, T##03, T##04, T##05, T##06, T##07, \
	T##08, T##09, T##0A, T##0B, T##0C, T##0D, T##0E, T##0F, \
	T##10, T##11, T##12, T##13, T##14, T##15, T##16, T##17, \
	T##18, T##19, T##1A, T##1B, T##1C, T##1D, T##1E, T##1F, \
	T##20, T##21, T##22, T##23, T##24, T##25, T##26, T##27, \
	T##28, T##29, T##2A, T##2B, T##2C, T##2D, T##2E, T##2F, \
	T##30, T##31, T##32, T##33, T##34, T##35, T##36, T##37, \
	T##38, T##39, T##3A, T##3B, T##3C, T##3D, T##3E, T##3F, \
	T##40, T##41, T##42, T##43, T##44, T##45, T##46, T##47, \
	T##48, T##49, T##4A, T##4B, T##4C, T##4D, T##4E, T##4F, \
	T##50, T##51, T##52, T##53, T##54, T##55, T##56, T##57, \
	T##58, T##59, T##5A, T##5B, T##5C, T##5D, T##5E, T##5F, \
	T##60, T##61, T##62, T##63, T##64, T##65, T##66, T##67, \
	T##68, T##69, T##6A, T##6B, T##6C, T##6D, T##6E, T##6F, \
	T##70, T##71, T##72, T##73, T##74, T##75, T##76, T##77, \
	T##78, T##79, T##7A, T##7B, T##7C, T##7D, T##7E, T##7F

execute_t FUNCT7(xops), FUNCT7(wops);
