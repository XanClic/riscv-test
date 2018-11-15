#include <stddef.h>
#include <stdio.h>


#define REF(visible_name, underscored_name) \
    do { \
        extern const void _binary_assets_ ## underscored_name ## _start; \
        extern const void _binary_assets_ ## underscored_name ## _size; \
        stdio_add_inode(visible_name, \
            &_binary_assets_ ## underscored_name ## _start, \
            (size_t)&_binary_assets_ ## underscored_name ## _size); \
    } while (0)


void init_incbinfs(void)
{
    REF("/abort.png", abort_png);
    REF("/army-none.png", army_none_png);
    REF("/army-blue-1.png", army_blue_1_png);
    REF("/army-blue-2.png", army_blue_2_png);
    REF("/army-blue-3.png", army_blue_3_png);
    REF("/army-blue-4.png", army_blue_4_png);
    REF("/army-red-1.png", army_red_1_png);
    REF("/army-red-2.png", army_red_2_png);
    REF("/army-red-3.png", army_red_3_png);
    REF("/army-red-4.png", army_red_4_png);
    REF("/bg.png", bg_png);
    REF("/cursor.png", cursor_png);
    REF("/focus-country.png", focus_country_png);
    REF("/loading.png", loading_png);
    REF("/music.ogg", music_ogg);
}
