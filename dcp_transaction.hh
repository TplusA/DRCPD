#ifndef DCP_TRANSACTION_HH
#define DCP_TRANSACTION_HH

#include <ostream>
#include <sstream>

class DcpTransaction
{
  private:
    std::ostream *os_;
    std::ostringstream sstr_;

    enum state
    {
        IDLE,
        WAIT_FOR_COMMIT,
        WAIT_FOR_ANSWER,
    };

    state state_;

  public:
    enum Result
    {
        OK = 0,
        FAILED = 1,
        INVALID_ANSWER = 2,
        IO_ERROR = 3,
    };

    DcpTransaction(const DcpTransaction &) = delete;
    DcpTransaction &operator=(const DcpTransaction &) = delete;

    explicit DcpTransaction():
        os_(nullptr),
        state_(IDLE)
    {}

    void set_output_stream(std::ostream *os)
    {
        os_ = os;
    }

    std::ostream *stream()
    {
        return state_ == WAIT_FOR_COMMIT ? &sstr_ : nullptr;
    }

    bool is_in_progress() const
    {
        return state_ != IDLE;
    }

    /*!
     * Start a transaction.
     */
    bool start();

    /*!
     * Commence sending the data, no further writes are allowed.
     */
    bool commit();

    /*!
     * Received and answer, the transaction is ended by this function.
     */
    bool done();

    /*!
     * Abort the transaction, do not send anything.
     */
    bool abort();
};

#endif /* !DCP_TRANSACTION_HH */
