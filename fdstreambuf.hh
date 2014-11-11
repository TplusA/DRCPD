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
