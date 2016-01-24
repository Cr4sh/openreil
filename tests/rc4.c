#include <stdio.h>
#include <string.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

typedef struct _RC4_CTX 
{
    unsigned char S[256];
    unsigned char x, y;

} RC4_CTX;

void rc4_swap(unsigned char *a, unsigned char *b)
{
    unsigned char c = *a;

    *a = *b;
    *b = c;
}

void rc4_set_key(RC4_CTX *ctx, unsigned char *key, int key_len)
{
    int i = 0;
    unsigned char x = 0, y = 0;    
    unsigned char *S = ctx->S;

    ctx->x = x = 0;
    ctx->y = y = 0;

    for (i = 0; i < 256; i++)
    {
        S[i] = (unsigned char)i;
    }
    
    for (i = 0; i < 256; i++)
    {
        y = (y + S[i] + key[x]) & 0xff;

        rc4_swap(&S[i], &S[y]);

        if (++x >= key_len)
        {
            x = 0;
        }
    }
}

void rc4_crypt(RC4_CTX *ctx, unsigned char *data, int data_len)
{
    int i = 0;
    unsigned char x = 0, y = 0;
    unsigned char *S = ctx->S;

    for (i = 0; i < data_len; i++)
    {
        x = (x + 1) & 0xff;
        y = (y + S[x]) & 0xff;

        rc4_swap(&S[x], &S[y]);

        data[i] ^= S[(S[x] + S[y]) & 0xff];
    }

    ctx->x = x;
    ctx->y = y;
}

#define MAX_DATA_LEN 255

RC4_CTX m_ctx;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        return -1;
    }

    int i = 0;
    unsigned char buff[MAX_DATA_LEN];
    char *key = argv[1], *data = argv[2];
    int key_len = strlen(key);
    int data_len = strlen(data);

    memcpy(buff, data, MIN(MAX_DATA_LEN, data_len));

    printf("\nKey: ");

    for (i = 0; i < key_len; i++)
    {
        printf("%.2x ", (unsigned char)key[i]);
    }

    printf("\n\nPlaintext: ");

    for (i = 0; i < data_len; i++)
    {
        printf("%.2x ", buff[i]);
    }
 
    // encrypt
    rc4_set_key(&m_ctx, (unsigned char *)key, key_len);
    rc4_crypt(&m_ctx, buff, data_len);

    printf("\n\nEncrypted: ");

    for (i = 0; i < data_len; i++)
    {
        printf("%.2x ", buff[i]);
    }

    // decrypt
    rc4_set_key(&m_ctx, (unsigned char *)key, key_len);
    rc4_crypt(&m_ctx, buff, data_len);

    printf("\n\nDecrypted: ");

    for (i = 0; i < data_len; i++)
    {
        printf("%.2x ", buff[i]);
    }

    printf("\n\n");

    return 0;
}
