#ifndef __LZW_H__

// не устанавливайте DICT_SIZE> 24 бит (32-битный буфер слишком короткий)
#define DICT_SIZE	(1 << 18)
#define CODE_NULL	(0x01000000)
#define HASH_SIZE	(DICT_SIZE)

#define LZW_ERR_DICT_IS_FULL	-1
#define LZW_ERR_INPUT_BUF		-2
#define LZW_ERR_WRONG_CODE		-3

#define DISABLE_ADD_NEW_NODE
#define OUTPUT_FILE
//#define OUTPUT_BUFFER

// бит-буфер
typedef struct _bitbuffer
{
    unsigned long buf;		// bits
    unsigned n;				// number of bits
}bitbuffer_t;

#pragma pack(push, 1)

// Узел кодировщика
typedef struct _node_lzw
{
    int           prev;		// prefix code
    int           next;		// next child code
    unsigned char ch;		// last symbol
}node_lzw_t;

#pragma pack(pop)

// LZW контекст кодера
typedef struct _lzw_enc
{
    int           code;				// current code
    unsigned      max;				// maximal code
    unsigned      codesize;			// number of bits in code
    bitbuffer_t   bb;				// bit-buffer struct
    void          *stream;			// pointer to the stream object
    unsigned      lzwn;				// output code-buffer byte counter
    node_lzw_t    dict[DICT_SIZE];	// code dictionary
    int           hash[HASH_SIZE];	// hash table
    unsigned char buff[256];		// output code-buffer
    unsigned char *e_buf;
    unsigned      e_size;
    unsigned      e_pos;
    char          en_dic;
}lzw_enc_t;

// LZW контекст декодера
typedef struct _lzw_dec
{
    int           code;				// current code
    unsigned      max;				// maximal code
    unsigned      codesize;			// number of bits in code
    bitbuffer_t   bb;				// bit-buffer struct
    void          *stream;			// pointer to the stream object
    unsigned      lzwn;				// input code-buffer byte counter
    unsigned      lzwm;				// input code-buffer size
    unsigned char *inbuff;		    // input code-buffer
    node_lzw_t    dict[DICT_SIZE];	// code dictionary
    unsigned char buff[DICT_SIZE];	// output string buffer
    unsigned char c;				// first char of the code
    unsigned char *e_buf;
    unsigned      e_size;
    unsigned      e_pos;
    char          en_dic;
}
lzw_dec_t;

void lzw_enc_init(lzw_enc_t *ctx, void *stream, char * buf, unsigned buf_size);
int  lzw_encode  (lzw_enc_t *ctx, char * buf, unsigned size);
void lzw_enc_end (lzw_enc_t *ctx);
void lzw_disable_update_dictionary(lzw_enc_t *ctx);

void lzw_dec_restore(lzw_dec_t *ctx, void *stream, char * buf, unsigned buf_size);
void lzw_dec_init(lzw_dec_t *ctx, void *stream, char * buf, unsigned buf_size);
int  lzw_decode  (lzw_dec_t *ctx, char * buf, unsigned size);

#ifdef OUTPUT_FILE
// Application defined stream callbacks
void     lzw_writebuf(void *stream, char *buf, unsigned size);
unsigned lzw_readbuf (void *stream, char *buf, unsigned size);
#endif

#endif //__LZW_H__