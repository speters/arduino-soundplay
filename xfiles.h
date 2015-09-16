/* Copyright (c) 2008 Carlos Lamas
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. */

#ifndef _XFILES_H_
#define _XFILES_H_ 1

#define __need_size_t
#include <stddef.h>


#if !defined(__STRINGIFY2)
#define __STRINGIFY2(__s) #__s
#define __STRINGIFY(__s) __STRINGIFY2(__s)
#endif


#define XF_START(__var) XF_ ## __var ## _start
#define XF_END(__var) XF_ ## __var ## _end
#define XF_SIZEb(__var) XF_ ## __var ## _size
#define XF_SIZE(__var) ((size_t)&XF_SIZEb(__var))


#define XF_INSERT_FILE(__var, __fileName)                          \
__asm__ (                                                          \
    ".pushsection .progmem"                              "\n\t"    \
    ".global " __STRINGIFY(XF_START(__var))              "\n\t"    \
    ".global " __STRINGIFY(XF_END(__var))                "\n\t"    \
    ".global " __STRINGIFY(XF_SIZEb(__var))              "\n\t"    \
    ".type " __STRINGIFY(XF_START(__var)) ", @object"    "\n\t"    \
    ".size " __STRINGIFY(XF_START(__var)) ", "                     \
           __STRINGIFY(XF_END(__var)) " - "                        \
           __STRINGIFY(XF_START(__var))                  "\n\t"    \
    __STRINGIFY(XF_START(__var)) ":"                     "\n\t"    \
    ".incbin \"" __fileName "\""                         "\n\t"    \
    __STRINGIFY(XF_END(__var)) ":"                       "\n\t"    \
    ".equiv " __STRINGIFY(XF_SIZEb(__var)) ", "                    \
           __STRINGIFY(XF_END(__var)) " - "                        \
           __STRINGIFY(XF_START(__var))                  "\n\t"    \
    ".popsection"                                                  \
)


#define XF_DECLARE(__var)               \
extern const char XF_START(__var)[];    \
extern const char XF_END(__var)[];      \
extern const void XF_SIZEb(__var)


#endif /* _XFILES_ */
