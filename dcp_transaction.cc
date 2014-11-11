#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "dcp_transaction.hh"

static void clear_stream(std::ostringstream &ss)
{
    ss.str("");
    ss.clear();
}

bool DcpTransaction::start()
{
    switch(state_)
    {
      case IDLE:
        break;

      case WAIT_FOR_COMMIT:
      case WAIT_FOR_ANSWER:
        return false;
    }

    clear_stream(sstr_);
    state_ = WAIT_FOR_COMMIT;

    return true;
}

bool DcpTransaction::commit()
{
    switch(state_)
    {
      case WAIT_FOR_COMMIT:
        break;

      case IDLE:
      case WAIT_FOR_ANSWER:
        return false;
    }

    if(!sstr_.str().empty())
    {
        if(os_ != nullptr)
        {
            *os_ << "Size: " << sstr_.str().length() << '\n' << sstr_.str();
            os_->flush();
        }

        clear_stream(sstr_);
    }

    state_ = WAIT_FOR_ANSWER;

    return true;
}

bool DcpTransaction::done()
{
    switch(state_)
    {
      case WAIT_FOR_ANSWER:
        break;

      case IDLE:
      case WAIT_FOR_COMMIT:
        return false;
    }

    clear_stream(sstr_);
    state_ = IDLE;

    return true;
}

bool DcpTransaction::abort()
{
    switch(state_)
    {
      case WAIT_FOR_COMMIT:
      case WAIT_FOR_ANSWER:
        break;

      case IDLE:
        return false;
    }

    state_ = WAIT_FOR_ANSWER;

    return done();
}
