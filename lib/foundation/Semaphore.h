/**
 *****************************************************************************

 @file       Semaphore.h

 @brief      Semaphore implementation

 @author     Hiroki Takada (TSoftware - http://www.tsoftware.jp)

 @date       2014-03-24

 @version    $Id:$


  Copyright 2014, 2015, 2016, 2017 Hiroki Takada

  This file is part of libshogi.

  libshogi is free software: you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the
  Free Software Foundation, either version 3 of the License, or (at your
  option) any later version.

  libshogi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with libshogi. If not, see <http://www.gnu.org/licenses/>.


  ----------------------------------------------------------------------------
  RELEASE NOTE :

   DATE          REV    REMARK
  ============= ====== =======================================================
  24th Mar 2014  0.1    Initial release
   2nd Oct 2026  0.2   Modified by 伊藤匠吾: ported to macOS

 *****************************************************************************/

#ifndef _FOUNDATION_SEMAPHORE_H
#define _FOUNDATION_SEMAPHORE_H

#include <cerrno>

// macOS does not support unnamed POSIX semaphores (sem_init() fails with
// ENOSYS). Use a mutex / condition variable implementation there (define
// LIBSHOGI_PORTABLE_SYNC to force it elsewhere).
#if defined(__APPLE__) || defined(LIBSHOGI_PORTABLE_SYNC)
#define _FOUNDATION_SEMAPHORE_PORTABLE
#include <mutex>
#include <condition_variable>
#else
#include <semaphore.h>
#endif

// begin namespace 'foundation'
namespace foundation
{

/* --------------------------- macro declaration --------------------------- */
#ifdef _FOUNDATION_SEMAPHORE_DEBUG
#define _FOUNDATION_SEMAPHORE_CHECK(x) \
    {                                  \
        assert((x));                   \
    }
#define _FOUNDATION_SEMAPHORE_DEBUG_ERROR_STRING_MAX 256
#define _FOUNDATION_SEMAPHORE_DEBUG_OUT(fmt, args...)                     \
    {                                                                     \
        fprintf(stderr, "FOUNDATION_SEMAPHORE_DEBUG     : " fmt, ##args); \
    }
#define _FOUNDATION_SEMAPHORE_DEBUG_FUNCTION_RESULT(x)                   \
    {                                                                    \
        char errorString_[_FOUNDATION_SEMAPHORE_DEBUG_ERROR_STRING_MAX]; \
        sprintf(errorString_, "%s - %s - %d", (x), __FILE__, __LINE__);  \
        perror(errorString_);                                            \
    }
#else
#define _FOUNDATION_SEMAPHORE_CHECK(x)
#define _FOUNDATION_SEMAPHORE_DEBUG_OUT(fmt, args...)
#define _FOUNDATION_SEMAPHORE_DEBUG_FUNCTION_RESULT(x)
#endif
    /* ------------------------------------------------------------------------- */

    /* ------------------------- prototype declaration ------------------------- */
    class Semaphore;
    /* ------------------------------------------------------------------------- */

    /* ---------------------------- semaphore class ---------------------------- */

    class Semaphore
    {

    public:
#ifdef _FOUNDATION_SEMAPHORE_PORTABLE

        /// constructor
        Semaphore() : _count(0) {}

        /// constructor
        Semaphore(unsigned int val) : _count(val) {}

        /// destructor
        virtual ~Semaphore() {}

        /// wait
        void wait(void)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [this]
                       { return _count > 0; });
            --_count;
        }

        /// trywait
        bool check(void)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_count > 0)
            {
                --_count;
                return true;
            }
            errno = EAGAIN;
            return false;
        }

        /// post
        void post(void)
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                ++_count;
            }
            _cond.notify_one();
        }

    private:
        /// void copy constructor
        Semaphore(const Semaphore &);

        /// void copy operator
        Semaphore &operator=(const Semaphore &);

        /// mutex
        std::mutex _mutex;

        /// condition variable
        std::condition_variable _cond;

        /// counter
        unsigned int _count;

#else

        /// constructor
        Semaphore()
        {
            sem_init(&_sem, 0, 0);
        }

        /// constructor
        Semaphore(unsigned int val)
        {
            sem_init(&_sem, 0, val);
        }

        /// destructor
        virtual ~Semaphore()
        {
            sem_destroy(&_sem);
        }

        /// wait
        void wait(void)
        {
            sem_wait(&_sem);
        }

        /// trywait
        bool check(void)
        {
            while (1)
            {
                if (sem_trywait(&_sem) == 0)
                {
                    return true;
                }
                if (errno == EAGAIN)
                {
                    return false;
                }
            }
        }

        /// post
        void post(void)
        {
            sem_post(&_sem);
        }

    private:
        /// void copy constructor
        Semaphore(const Semaphore &);

        /// void copy operator
        Semaphore &operator=(const Semaphore &);

        /// semaphore
        sem_t _sem;

#endif
    };

    /* --------------------------------------------------------------- */

    // end namespace 'foundation'
}

#endif
