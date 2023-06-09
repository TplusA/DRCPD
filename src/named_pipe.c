/*
 * Copyright (C) 2015, 2016, 2019, 2022  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "named_pipe.h"
#include "messages.h"
#include "os.h"

int fifo_create_and_open(const char *devname, bool write_not_read)
{
    int ret = mkfifo(devname, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

    if(ret < 0)
    {
        if(errno != EEXIST)
        {
            msg_error(errno, LOG_EMERG,
                      "Failed creating named pipe \"%s\"", devname);
            return -1;
        }
    }

    return fifo_open(devname, write_not_read);
}

int fifo_open(const char *devname, bool write_not_read)
{
    int ret = open(devname, write_not_read ? O_WRONLY : (O_RDONLY | O_NONBLOCK));

    if(ret < 0)
        msg_error(errno, LOG_EMERG,
                  "Failed opening named pipe \"%s\"", devname);
    else
        msg_vinfo(MESSAGE_LEVEL_TRACE, "Opened %sable pipe \"%s\", fd %d",
                  write_not_read ? "writ" : "read", devname, ret);

    return ret;
}

void fifo_close(int *fd)
{
    int ret;

    while((ret = close(*fd)) < 0 && errno == EINTR)
        ;

    if(ret < 0)
        msg_error(errno, LOG_ERR, "Failed closing named pipe fd %d", *fd);

    *fd = -1;
}

void fifo_close_and_delete(int *fd, const char *devname)
{
    fifo_close(fd);

    if(unlink(devname) < 0)
        msg_error(errno, LOG_ERR,
                  "Failed deleting named pipe \"%s\"", devname);
}

bool fifo_reopen(int *fd, const char *devname, bool write_not_read)
{
    fifo_close(fd);
    *fd = fifo_open(devname, write_not_read);
    return *fd >= 0;
}

int fifo_write_from_buffer(const uint8_t *src, size_t count, int fd)
{
    while(count > 0)
    {
        ssize_t len = os_write(fd, src, count);

        if(len < 0)
        {
            msg_error(errno, LOG_ERR, "Failed writing to fd %d", fd);
            return -1;
        }

        msg_log_assert((size_t)len <= count);

        src += len;
        count -= len;
    }

    return 0;
}

int fifo_try_read_to_buffer(uint8_t *dest, size_t count, size_t *dest_pos,
                            int fd)
{
    dest += *dest_pos;
    count -= *dest_pos;

    int retval = 0;

    while(count > 0)
    {
        const ssize_t len = os_read(fd, dest, count);

        if(len == 0)
            break;

        if(len < 0)
        {
            retval = (errno == EAGAIN) ? 0 : -1;
            msg_error(errno, LOG_ERR, "Failed reading from fd %d", fd);
            break;
        }

        msg_log_assert((size_t)len <= count);

        dest += len;
        count -= len;
        *dest_pos += len;
        retval = 1;
    }

    return retval;
}
