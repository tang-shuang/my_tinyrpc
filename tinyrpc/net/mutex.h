#ifndef TINYRPC_NET_MUTEX_H
#define TINYRPC_NET_MUTEX_H

#include <pthread.h>
#include <memory>

#include <queue>

#include "tinyrpc/coroutine/coroutine.h"

namespace tinyrpc
{
    template <class T>
    struct ScopedLockImpl
    {
    public:
        ScopedLockImpl(T &mutex);
        ~ScopedLockImpl();

        void lock();
        void unlock();

    private:
        /// mutex
        T &m_mutex;
        /// 是否已上锁
        bool m_locked;
    };

    template <class T>
    struct ReadScopedLockImpl
    {
    public:
        ReadScopedLockImpl(T &mutex);
        ~ReadScopedLockImpl();

        void lock();
        void unlock();

    private:
        /// mutex
        T &m_mutex;
        /// 是否已上锁
        bool m_locked;
    };

    /**
     * @brief 局部写锁模板实现
     */
    template <class T>
    struct WriteScopedLockImpl
    {
    public:
        WriteScopedLockImpl(T &mutex);
        ~WriteScopedLockImpl();

        void lock();
        void unlock();

    private:
        T &m_mutex;
        bool m_locked;
    };

    class Mutex
    {
    public:
        /// 局部锁
        typedef ScopedLockImpl<Mutex> Lock;

        Mutex();
        ~Mutex();

        void lock();
        void unlock();
        pthread_mutex_t *getMutex();

    private:
        /// mutex
        pthread_mutex_t m_mutex;
    };

    class RWMutex
    {
    public:
        /// 局部读锁
        typedef ReadScopedLockImpl<RWMutex> ReadLock;

        typedef WriteScopedLockImpl<RWMutex> WriteLock;

        RWMutex();
        ~RWMutex();

        void rdlock();
        void wrlock();
        void unlock();

    private:
        pthread_rwlock_t m_lock;
    };

    class CoroutineMutex
    {
    public:
        typedef ScopedLockImpl<CoroutineMutex> Lock;

        CoroutineMutex();
        ~CoroutineMutex();

        void lock();
        void unlock();

    private:
        bool m_lock{false};
        Mutex m_mutex;
        std::queue<Coroutine *> m_sleep_cors;
    };
}
#endif