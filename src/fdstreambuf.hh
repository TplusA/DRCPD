/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef FDSTREAMBUF_HH
#define FDSTREAMBUF_HH

#include <streambuf>

#include "named_pipe.h"
#include "messages.h"

/*!
 * Stream buffer that writes to a file descriptor.
 *
 * Use like this:
 * \code
 * int fd = open("/dev/null", O_WRONLY, 0);
 * FdStreambuf fd_sbuf(fd);
 * std::ostream fd_out(&fd_sbuf);
 * fd_out << "Hello world!" << std::endl;
 * \endcode
 */
class FdStreambuf: public std::streambuf
{
  private:
    int fd_;

  public:
    FdStreambuf(const FdStreambuf &) = delete;
    FdStreambuf &operator=(const FdStreambuf &) = delete;

    explicit FdStreambuf(int fd = -1): fd_(fd) {}
    virtual ~FdStreambuf() {}

    virtual std::streamsize xsputn(const char_type *s, std::streamsize count) override
    {
        if(fd_ >= 0)
            return ((fifo_write_from_buffer(reinterpret_cast<const uint8_t *>(s),
                                            count * sizeof(char_type), fd_) == 0)
                    ? count
                    : 0);

        msg_error(EINVAL, LOG_CRIT,
                  "Attempted to write %zu bytes, but fd not set",
                  count * sizeof(char_type));

        return 0;
    }

    void set_fd(int fd)
    {
        fd_ = (fd >= 0) ? fd : -1;
    }
};

#endif /* !FDSTREAMBUF_HH */
