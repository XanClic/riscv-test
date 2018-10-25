#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static struct Inode *inodes;
static int inode_count;


void stdio_add_inode(const char *name, const void *base, size_t size)
{
    inodes = realloc(inodes, ++inode_count * sizeof(inodes[0]));
    inodes[inode_count - 1] = (struct Inode){
        .name = name,
        .base = base,
        .size = size,
    };
}


FILE *fopen(const char *path, const char *mode)
{
    (void)mode;

    for (int i = 0; i < inode_count; i++) {
        if (!strcmp(path, inodes[i].name)) {
            FILE *fp = malloc(sizeof(*fp));
            *fp = (FILE){
                .inode = &inodes[i],
                .loc = 0,
            };
            return fp;
        }
    }

    return NULL;
}


int fclose(FILE *stream)
{
    free(stream);
    return 0;
}


size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t byte_size = size * nmemb; // FIXME

    if ((size_t)stream->loc > stream->inode->size) {
        return 0;
    }

    if (byte_size > stream->inode->size - stream->loc) {
        byte_size = stream->inode->size - stream->loc;
    }
    nmemb = byte_size / size;
    byte_size = nmemb * size;

    memcpy(ptr, (char *)stream->inode->base + stream->loc, byte_size);
    stream->loc += byte_size;

    return nmemb;
}


long ftell(FILE *stream)
{
    return stream->loc;
}


int fseek(FILE *stream, long offset, int whence)
{
    switch (whence) {
        case SEEK_SET:
            stream->loc = offset;
            break;

        case SEEK_CUR:
            if (stream->loc + offset < 0) {
                errno = EINVAL;
                return -1;
            }
            stream->loc += offset;
            break;

        case SEEK_END:
            if ((long)stream->inode->size + offset < 0) {
                errno = EINVAL;
                return -1;
            }
            stream->loc = stream->inode->size + offset;
            break;

        default:
            errno = EINVAL;
            return -1;
    }

    return 0;
}
