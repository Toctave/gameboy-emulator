#include "handmade_keyboard.h"

#define DO_KEY_INDEX_STRING(index, name) #name,

const char* KeyIndex_names[] = {
    FOR_ALL_KEY_INDICES(DO_KEY_INDEX_STRING)
};
