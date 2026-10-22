/* Minimal os_support.h for RNNoise 0.2 (added by Sunai).
   The v0.2 sources include this header from vec_neon.h but it is not part of
   the release; these are the only definitions the ARM code uses. */
#ifndef OS_SUPPORT_H
#define OS_SUPPORT_H

#include <string.h>
#include <stdlib.h>
#include "opus_types.h"
#include "common.h"

#ifndef OPUS_CLEAR
#define OPUS_CLEAR(dst, n) (memset((dst), 0, (n)*sizeof(*(dst))))
#endif
#ifndef OPUS_COPY
#define OPUS_COPY(dst, src, n) (memcpy((dst), (src), (n)*sizeof(*(dst)) + 0*((dst)-(src))))
#endif
#ifndef OPUS_MOVE
#define OPUS_MOVE(dst, src, n) (memmove((dst), (src), (n)*sizeof(*(dst)) + 0*((dst)-(src))))
#endif

#endif
