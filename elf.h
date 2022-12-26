/* File header */

#define  EI_NIDENT   16
/* uint8_t ident[EI_NIDENT]; */
#define  EI_MAG0     0
#define  ELFMAG0     0x7F
#define  EI_MAG1     1
#define  ELFMAG1     0x45 /* E */
#define  EI_MAG2     2
#define  ELFMAG2     0x4C /* L */
#define  EI_MAG3     3
#define  ELFMAG3     0x46 /* F */
#define  EI_CLASS    4
#define  ELFCLASS32  1
#define  ELFCLASS64  2
#define  EI_DATA     5
#define  ELFDATA2LSB 1
#define  EI_VERSION  6
/* uint16_t type; */
#define  ET_NONE     0
#define  ET_EXEC     2
/* uint16_t machine; */
#define  EM_NONE     0
#define  EM_RISCV    243
/* uint32_t version; */
#define  EV_NONE     0
#define  EV_CURRENT  1
/* xword_t entry, phoff, shoff; */
/* uint32_t flags; */
/* uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx; */
