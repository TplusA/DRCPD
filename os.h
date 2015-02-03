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

#ifndef OS_H
#define OS_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

extern ssize_t (*os_read)(int fd, void *dest, size_t count);
extern ssize_t (*os_write)(int fd, const void *buf, size_t count);

void os_abort(void);

#ifdef __cplusplus
}
#endif

#endif /* !OS_H */
