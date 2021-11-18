#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace BFA
{
	typedef struct {
		unsigned char *BytePtr;
		unsigned int  BitPos;
	} huff_bitstream_t;
	typedef struct {
		int Symbol;
		unsigned int Count;
		unsigned int Code;
		unsigned int Bits;
	} huff_sym_t;
	typedef struct {
		unsigned char *BytePtr;
		unsigned int  BitPos;
		unsigned int  NumBytes;
	} rice_bitstream_t;
	typedef struct {
		unsigned char *BytePtr;
		unsigned int  BitPos;
	} sf_bitstream_t;
	typedef struct {
		unsigned int Symbol;
		unsigned int Count;
		unsigned int Code;
		unsigned int Bits;
	} sf_sym_t;
	
	typedef struct huff_encodenode_struct huff_encodenode_t;
	struct huff_encodenode_struct {
		huff_encodenode_t *ChildA, *ChildB;
		int Count;
		int Symbol;
	};
	typedef struct huff_decodenode_struct huff_decodenode_t;
	struct huff_decodenode_struct {
		huff_decodenode_t *ChildA, *ChildB;
		int Symbol;
	};
	typedef struct sf_treenode_struct sf_treenode_t;
	struct sf_treenode_struct {
		sf_treenode_t *ChildA, *ChildB;
		int Symbol;
	};

	#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
	/* Windows */
		#include <windows.h>
		#define WINDOWS_TIMER
		typedef struct {
			double  period;
			__int64 t0;
		} systimer_t;
	#elif defined(__DJGPP__)
	/* DOS/DJGPP */
		#include <time.h>
		#define DOS_TIMER
		typedef struct {
			uclock_t t0;
		} systimer_t;
	#else
	/* Unix (Linux etc) */
		#include <stdlib.h>
		#include <sys/time.h>
		#define GTOD_TIMER
		typedef struct {
			struct timeval t0;
		} systimer_t;
	#endif

	#define MAX_TREE_NODES 511// The maximum number of nodes in the Huffman tree is 2^(8+1)-1 = 511 
	
	/* Maximum offset (can be any size < 2^31). Lower values give faster
    compression, while higher values gives better compression. The default
    value of 100000 is quite high. Experiment to see what works best for you. */
	#define LZ_MAX_OFFSET 100000
	#define RICE_FMT_INT8   1  /* signed 8-bit integer    */
	#define RICE_FMT_UINT8  2  /* unsigned 8-bit integer  */
	#define RICE_FMT_INT16  3  /* signed 16-bit integer   */
	#define RICE_FMT_UINT16 4  /* unsigned 16-bit integer */
	#define RICE_FMT_INT32  7  /* signed 32-bit integer   */
	#define RICE_FMT_UINT32 8  /* unsigned 32-bit integer */
	#define RICE_HISTORY	16 /* Number of words to use for determining the optimum k */
	#define RICE_THRESHOLD	8  /* Maximum length of Rice codes */
	

	class BasicFileAlgs
	{
	public:
		BasicFileAlgs(void);
		int BasicFile(int argc, char *argv[]);
	};
}
