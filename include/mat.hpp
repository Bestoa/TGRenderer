#ifndef __TOPGUN_MAT__
#define __TOPGUN_MAT__

namespace TGRenderer
{
    enum MAT3_SYSTEM_TYPE
    {
        MAT3_NORMAL,
        MAT3_SYSTEM_TYPE_MAX,
    };

    enum MAT4_SYSTEM_TYPE
    {
        MAT4_MODEL,
        MAT4_VIEW,
        MAT4_PROJ,
        MAT4_MODELVIEW,
        MAT4_MVP,
        MAT4_LIGHT_MVP,
        MAT4_SYSTEM_TYPE_MAX,
    };

    enum MAT_INDEX
    {
        MAT0, MAT1, MAT2, MAT3, MAT4, MAT5, MAT6, MAT7, MAT8, MAT9, MAT10, MAT11, MAT12, MAT13, MAT14, MAT15,
        MAT_INDEX_MAX,
    };

    const int MAT3_USER_0 = MAT_INDEX_MAX - MAT3_SYSTEM_TYPE_MAX;
    const int MAT3_USER_MAX = MAT_INDEX_MAX - MAT3_USER_0;
    const int MAT4_USER_0 = MAT_INDEX_MAX - MAT4_SYSTEM_TYPE_MAX;
    const int MAT4_USER_MAX = MAT_INDEX_MAX - MAT4_USER_0;

    typedef int MAT_INDEX_TYPE;
}
#endif
