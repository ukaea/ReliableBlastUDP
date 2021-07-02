#pragma once
#define RSE_DEBUG

#ifdef RSE_DEBUG
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...) ((void)0)
#endif