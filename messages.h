/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * DRCPD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * DRCPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DRCPD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdbool.h>
#include <syslog.h>

#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Whether or not to make use of syslog.
 */
void msg_enable_syslog(bool enable_syslog);

/*!
 * Emit error to stderr and syslog.
 *
 * \param error_code The current error code as stored in errno.
 * \param priority A log priority as expected by syslog(3).
 * \param error_format Format string followed by arguments.
 */
void msg_error(int error_code, int priority, const char *error_format, ...)
    __attribute__ ((format (printf, 3, 4)));

/*
 * Emit log informative message to stderr and syslog.
 */
void msg_info(const char *format_string, ...)
    __attribute__ ((format (printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#define BUG(...) msg_error(0, LOG_CRIT, "BUG: " __VA_ARGS__)

#ifdef NDEBUG
#define log_assert(EXPR) do {} while(0)
#else /* !NDEBUG */
#define log_assert(EXPR) \
    do \
    { \
        if(!(EXPR)) \
        { \
            msg_error(0, LOG_EMERG, "Assertion failed at %s:%d: " #EXPR, \
                      __FILE__, __LINE__); \
            os_abort(); \
        } \
    } \
    while(0)
#endif /* NDEBUG */

#endif /* !MESSAGES_H */
