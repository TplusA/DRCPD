/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef NAMED_PIPE_H
#define NAMED_PIPE_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

struct fifo_pair
{
    int in_fd;
    int out_fd;
};

#ifdef __cplusplus
extern "C" {
#endif

int fifo_create_and_open(const char *devname, bool write_not_read);
int fifo_open(const char *devname, bool write_not_read);
void fifo_close_and_delete(int *fd, const char *devname);
void fifo_close(int *fd);
bool fifo_reopen(int *fd, const char *devname, bool write_not_read);
int fifo_write_from_buffer(const uint8_t *src, size_t count, int fd);
int fifo_try_read_to_buffer(uint8_t *dest, size_t count,
                            size_t *dest_pos, int fd);

#ifdef __cplusplus
}
#endif

#endif /* !NAMED_PIPE_H */
