#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "lzw.h"

#define BUF_SIZE        2

void lzw_writebuf(void *stream, char *buf, unsigned size)
{
    fwrite(buf, size, 1, (FILE*)stream);
}

unsigned lzw_readbuf(void *stream, char *buf, unsigned size)
{
    return fread(buf, 1, size, (FILE*)stream);
}


// global object
lzw_dec_t lzwd;

/******************************************************************************
**  main
**  --------------------------------------------------------------------------
**  Decodes input LZW code stream into byte stream.
**
**  Arguments:
**      argv[1] - input file name;
**      argv[2] - output file name;
**
**  Return: error code
******************************************************************************/

int read_file_to_buffer(void * buf, size_t elem_size, FILE *file )
{
    fseek(file, 0, SEEK_SET);
    int data_size = 0;
    fread(&data_size, sizeof(int), 1, file);
    fread(buf, elem_size, data_size, file);
    return data_size;
}

int main (int argc, char* argv[])
{
    FILE       *fin;
    FILE       *fout;
    lzw_dec_t  *ctx = &lzwd;
    unsigned   len;
    char       buf[256];

    if (argc < 2) {
        printf("Usage: lzw-enc <input file> <output file>\n");
        return -1;
    }

    if (!(fin = fopen(argv[1], "rb"))) {
        fprintf(stderr, "Cannot open %s\n", argv[1]);
        return -2;
    }

    if (!(fout = fopen(argv[2], "w+b"))) {
        fprintf(stderr, "Cannot open %s\n", argv[2]);
        return -6;
    }

    printf("open files successful\n");

#ifndef DISABLE_ADD_NEW_NODE
    lzw_dec_init(ctx, fout, buf, BUF_SIZE);
#else
    FILE       *fdic;
    if (!(fdic = fopen(argv[3], "rb"))) {
        fprintf(stderr, "Cannot open %s\n", argv[3]);
        return -3;
    }
    read_file_to_buffer(ctx->dict, sizeof(node_lzw_t), fdic);
    //read_file_to_buffer(ctx->hash, sizeof(int), DICT_SIZE, fhash);
    fclose(fdic);
    lzw_dec_restore(ctx, fout, buf, BUF_SIZE);

#endif
    while (len = lzw_readbuf(fin, buf, sizeof(buf)))
    {
        int ret = lzw_decode(ctx, buf, len);

        if (ret == 0)
        {
            break;
        }

        if (ret != len)
        {
            fprintf(stderr, "Error %d\n", ret);
            break;
        }
    }

    fclose(fin);
    fclose(fout);

    return 0;
}