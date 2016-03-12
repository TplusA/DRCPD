/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYBACK_ABORT_ENQUEUE_HH
#define PLAYBACK_ABORT_ENQUEUE_HH

namespace Playback
{

class AbortEnqueueIface
{
  public:
    class TemporaryDataUnlock
    {
      private:
        AbortEnqueueIface *abort_enqueue_;

      public:
        explicit TemporaryDataUnlock(AbortEnqueueIface &abort_enqueue):
            abort_enqueue_(&abort_enqueue)
        {
            abort_enqueue_->unlock();
        }

        ~TemporaryDataUnlock()
        {
            abort_enqueue_->lock();
        }
    };

    class EnqueuingInProgressMarker
    {
      private:
        AbortEnqueueIface *abort_enqueue_;

      public:
        explicit EnqueuingInProgressMarker(AbortEnqueueIface &abort_enqueue):
            abort_enqueue_(&abort_enqueue)
        {
            abort_enqueue_->enqueue_start();
        }

        ~EnqueuingInProgressMarker()
        {
            abort_enqueue_->enqueue_stop();
        }
    };

  protected:
    explicit AbortEnqueueIface() {}

  public:
    AbortEnqueueIface(const AbortEnqueueIface &) = delete;
    AbortEnqueueIface &operator=(const AbortEnqueueIface &) = delete;

    virtual ~AbortEnqueueIface() {}

    virtual bool may_continue() const = 0;

    TemporaryDataUnlock temporary_data_unlock()
    {
        return TemporaryDataUnlock(*this);
    }

    EnqueuingInProgressMarker enqueuing_in_progress()
    {
        return EnqueuingInProgressMarker(*this);
    }

  private:
    virtual void unlock() = 0;
    virtual void lock() = 0;
    virtual bool enqueue_start() = 0;
    virtual bool enqueue_stop() = 0;
};

}

#endif /* !PLAYBACK_ABORT_ENQUEUE_HH */
