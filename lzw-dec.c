#include "lzw.h"


/**
 * \brief Прочитать биты из битового буфера.
 * \param ctx - указатель на контекст LZW
 * \param nbits - количество бит для чтения, 0-24
 * \return биты или -1, если данных нет
 */
static int lzw_dec_readbits(lzw_dec_t *const ctx, unsigned nbits)
{
    while (ctx->bb.n < nbits)
    {
        if (ctx->lzwn == ctx->lzwm)
            return -1;

        ctx->bb.buf = (ctx->bb.buf << 8) | ctx->inbuff[ctx->lzwn++];
        ctx->bb.n += 8;
    }

    ctx->bb.n -= nbits;

    return (ctx->bb.buf >> ctx->bb.n) & ((1 << nbits)-1);
}


void lzw_dec_restore(lzw_dec_t *ctx, void *stream, char * buf, unsigned buf_size)
{
    ctx->code     = CODE_NULL; // non-existent code
    ctx->max      = 0;
    ctx->codesize = 17;
    ctx->bb.n     = 0; // bitbuffer init
    ctx->e_buf    = buf;
    ctx->e_size   = buf_size;
    ctx->e_pos    = 0;
    ctx->stream   = stream;
    ctx->en_dic   = 0;
}

/**
 * \brief Инициализирует контекст декодера LZW
 * \param ctx - контекст декодера LZW
 * \param nbits - Указатель на объект потока ввода / вывода, определенный приложением
 */
void lzw_dec_init(lzw_dec_t *ctx, void *stream, char * buf, unsigned buf_size)
{
    unsigned i;

    ctx->code     = CODE_NULL;
    ctx->max      = 255;
    ctx->codesize = 8;
    ctx->bb.n     = 0; // bitbuffer init
    ctx->e_buf    = buf;
    ctx->e_size   = buf_size;
    ctx->e_pos    = 0;
    ctx->stream   = stream;
    ctx->en_dic   = 1;

    for (i = 0; i < 256; i++)
    {
        ctx->dict[i].prev = CODE_NULL;
        ctx->dict[i].ch   = i;
    }
}

/**
 * \brief Сбросить контекст декодера LZW. Используется при переполнении словаря.
 * Размер кода установлен на 8 бит. В этой ситуации code и output str равны.
 * \param ctx - контекст декодера LZW
 */
static void lzw_dec_reset(lzw_dec_t *const ctx)
{
    ctx->code     = CODE_NULL;
    ctx->max      = 255;
    ctx->codesize = 8;
#if DEBUG
    printf("reset\n");
#endif
}

/**
 * \brief Читает строку из словаря LZW. Из-за особой специфичности буфера он заполняется с конца,
 * поэтому смещение от начала буфера будет <размер буфера> - <размер строки>.
 * Размер кода установлен на 8 бит. В этой ситуации code и output str равны.
 * \param ctx - контекст декодера LZW
 * \param code - код строки (уже в словаре)
 */
static unsigned lzw_dec_getstr(lzw_dec_t *const ctx, int code)
{
    unsigned i = sizeof(ctx->buff);

    while (code != CODE_NULL && i)
    {
        ctx->buff[--i] = ctx->dict[code].ch;
        code = ctx->dict[code].prev;
    }

    return sizeof(ctx->buff) - i;
}

/**
 * \brief Добавляет строку в словарь LZW
 * \param ctx - контекст декодера LZW
 * \param code - код строки (уже в словаре)
 * \param c - последний символ
 */
static int lzw_dec_addstr(lzw_dec_t *const ctx, int code, unsigned char c)
{
    if (code == CODE_NULL)
        return c;

    if (++ctx->max == CODE_NULL)
        return CODE_NULL;

    ctx->dict[ctx->max].prev = code;
    ctx->dict[ctx->max].ch   = c;
#if DEBUG
    printf("add code %x = %x + %c\n", ctx->max, code, c);
#endif

    return ctx->max;
}

/**
 * \brief Записывает строку, представленную кодом, в выходной поток.
 * \param ctx - контекст декодера LZW
 * \param code - код строки (уже в словаре)
 */
static unsigned char lzw_dec_writestr(lzw_dec_t *const ctx, int code)
{
    unsigned strlen = lzw_dec_getstr(ctx, code);

#ifdef OUTPUT_BUFFER
    unsigned char * p_c = ctx->buff+(sizeof(ctx->buff) - strlen);
    for (int i = 0; i < strlen; i++)
    {
        if (ctx->e_pos < ctx->e_size)
        {
            ctx->e_buf[ctx->e_pos++] = *p_c++;
        }
    }
#endif

#ifdef OUTPUT_FILE
    // write the string into the output stream
    lzw_writebuf(ctx->stream, ctx->buff+(sizeof(ctx->buff) - strlen), strlen);
#endif

    return ctx->buff[sizeof(ctx->buff) - strlen];
}

/**
 * \brief Декодирует буфер кодов LZW и записывает строки в выходной поток.
 * Выходные данные записываются обратным вызовом приложения в
 * \param ctx - контекст декодера LZW
 * \param buf - буфер входного кода
 * \param size - размер буфера
 */
int lzw_decode(lzw_dec_t *ctx, char * buf, unsigned size)
{
    if (!size) return 0;

    ctx->inbuff = buf;	// save ptr to code-buffer
    ctx->lzwn   = 0;	// current position in code-buffer
    ctx->lzwm   = size;	// code-buffer data size

    for (;;)
    {
        int ncode;

        // read a code from the input buffer (ctx->inbuff[])
        ncode = lzw_dec_readbits(ctx, ctx->codesize);
#if DEBUG
        printf("code %x (%d)\n", ncode, ctx->codesize);
#endif

        // check the input for EOF
        if (ncode < 0)
        {
#if DEBUG
            if (ctx->lzwn != ctx->lzwm)
				return LZW_ERR_INPUT_BUF;
#endif
            break;
        }

#ifndef DISABLE_ADD_NEW_NODE
        else if (ncode <= ctx->max) // known code
        {
            // output string for the new code from dictionary
            ctx->c = lzw_dec_writestr(ctx, ncode);

            // add <prev code str>+<first str symbol> to the dictionary
            if (lzw_dec_addstr(ctx, ctx->code, ctx->c) == CODE_NULL)
                return LZW_ERR_DICT_IS_FULL;
        }
        else // unknown code
        {
            // try to guess the code
            if (ncode != ctx->max+1)
                return LZW_ERR_WRONG_CODE;

            // create code: <nc> = <code> + <c> wich is equal to ncode
            if (lzw_dec_addstr(ctx, ctx->code, ctx->c) == CODE_NULL)
                return LZW_ERR_DICT_IS_FULL;

            // output string for the new code from dictionary
            ctx->c = lzw_dec_writestr(ctx, ncode);
        }
#else
        ctx->c = lzw_dec_writestr(ctx, ncode);
#endif

        ctx->code = ncode;

#ifndef DISABLE_ADD_NEW_NODE
        // Увеличить размер кода (количество бит) если необходимо
        if (ctx->max+1 == (1 << ctx->codesize))
            ctx->codesize++;

        // Проверить не переполнился ли словарь
        if (ctx->max+1 == DICT_SIZE)
            lzw_dec_reset(ctx);
#endif
    }

    return ctx->lzwn;
}