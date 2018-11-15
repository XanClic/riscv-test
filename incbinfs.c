#include <stddef.h>
#include <stdio.h>


#define REF(visible_name, underscored_name) \
    do { \
        extern const void _binary_ ## underscored_name ## _start; \
        extern const void _binary_ ## underscored_name ## _size; \
        stdio_add_inode(visible_name, \
                        &_binary_ ## underscored_name ## _start, \
                        (size_t)&_binary_ ## underscored_name ## _size); \
    } while (0)


void init_incbinfs(void)
{
    REF("/bg.png", bg_png);
    REF("/loading.png", loading_png);
    REF("/music.ogg", music_ogg);
}
