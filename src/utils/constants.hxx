/** \copyright
 * Copyright (c) 2014, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file constants.hxx
 * Utility to specify linking-time constants and overrides for them.
 *
 * @author Balazs Racz
 * @date 30 Apr 2014
 */

#ifndef _UTILS_CONSTANTS_HXX_
#define _UTILS_CONSTANTS_HXX_

#include <stddef.h>

#ifdef __cplusplus
#define EXTERNC extern "C" {
#define EXTERNCEND }
#else
#define EXTERNC
#define EXTERNCEND
#endif

#define DECLARE_CONST(name)                                                    \
    EXTERNC extern void _sym_##name(void);                                     \
    EXTERNCEND typedef unsigned char                                           \
    _do_not_add_declare_and_default_const_to_the_same_file_for_##name;         \
    static inline ptrdiff_t config_##name(void)                                \
    {                                                                          \
        return (ptrdiff_t)(&_sym_##name);                                      \
    }

#define DEFAULT_CONST(name, value)                                             \
    typedef signed char                                                        \
    _do_not_add_declare_and_default_const_to_the_same_file_for_##name;         \
    asm(".global _sym_" #name " \n");                                          \
    asm(".weak _sym_" #name " \n");                                            \
    asm(".set _sym_" #name ", " #value " \n");

#define OVERRIDE_CONST(name, value)                                            \
    asm(".global _sym_" #name " \n");                                          \
    asm(".set _sym_" #name ", " #value " \n");

#endif // _UTILS_CONSTANTS_HXX_