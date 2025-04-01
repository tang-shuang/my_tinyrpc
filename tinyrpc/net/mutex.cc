#include "tinyrpc/net/mutex.h"

#include <pthread.h>
#include <memory>

#include "tinyrpc/net/reactor.h"
#include "tinyrpc/comm/log.h"
#include "tinyrpc/coroutine/coroutine.h"
#include "tinyrpc/coroutine/coroutine_hook.h"

namespace tinyrpc
{
    template <class T>
    ScopedLockImpl::ScopedLockImpl(T &mutex)
        : m_mutex(mutex)
    {
        m_mutex.lock();
        m_locked = true;
    }

    ScopedLockImpl::~ScopedLockImpl()
    {
        unlock();
    }

    void ScopedLockImpl::lock()
    {
        if (!m_locked)
        {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void ScopedLockImpl::unlock()
    {
        if (m_locked)
        {
            m_mutex.unlock();
            m_locked = false;
        }
    }

    template <class T>
    ReadScopedLockImpl::ReadScopedLockImpl(T &mutex)
        : m_mutex(mutex)
    {
        m_mutex.rdlock();
        m_locked = true;
    }

    ReadScopedLockImpl::~ReadScopedLockImpl()
    {
        unlock();
    }

    void ReadScopedLockImpl::lock()
    {
        if (!m_locked)
        {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void ReadScopedLockImpl::unlock()
    {
        if (m_locked)
        {
            m_mutex.unlock();
            m_locked = false;
        }
    }

    template <class T>
    WriteScopedLockImpl::WriteScopedLockImpl(T &mutex)
        : m_mutex(mutex)
    {
        m_mutex.wrlock();
        m_locked = true;
    }

    WriteScopedLockImpl::~WriteScopedLockImpl()
    {
        unlock();
    }

    void WriteScopedLockImpl::lock()
    {
        if (!m_locked)
        {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void WriteScopedLockImpl::unlock()
    {
        if (m_locked)
        {
            m_mutex.unlock();
            m_locked = false;
        }
    }

    Mutex::Mutex()
    {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    Mutex::~Mutex()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    void Mutex::lock()
    {
        pthread_mutex_lock(&m_mutex);
    }

    void Mutex::unlock()
    {
        pthread_mutex_unlock(&m_mutex);
    }

    pthread_mutex_t *Mutex::getMutex()
    {
        return &m_mutex;
    }

    RWMutex::RWMutex()
    {
        pthread_rwlock_init(&m_lock, nullptr);
    }

    RWMutex::~RWMutex()
    {
        pthread_rwlock_destroy(&m_lock);
    }

    void RWMutex::rdlock()
    {
        pthread_rwlock_rdlock(&m_lock);
    }

    void RWMutex::wrlock()
    {
        pthread_rwlock_wrlock(&m_lock);
    }

    void RWMutex::unlock()
    {
        pthread_rwlock_unlock(&m_lock);
    }

    CoroutineMutex::CoroutineMutex() {}

    CoroutineMutex::~CoroutineMutex()
    {
        if (m_lock)
        {
            unlock();
        }
    }

    void CoroutineMutex::lock()
    {

        if (Coroutine::IsMainCoroutine())
        {
            ErrorLog << "main coroutine can't use coroutine mutex";
            return;
        }

        Coroutine *cor = Coroutine::GetCurrentCoroutine();

        Mutex::Lock lock(m_mutex);
        if (!m_lock)
        {
            m_lock = true;
            DebugLog << "coroutine succ get coroutine mutex";
            lock.unlock();
        }
        else
        {
            m_sleep_cors.push(cor);
            auto tmp = m_sleep_cors;
            lock.unlock();

            DebugLog << "coroutine yield, pending coroutine mutex, current sleep queue exist ["
                     << tmp.size() << "] coroutines";

            Coroutine::Yield();
        }
    }

    void CoroutineMutex::unlock()
    {
        if (Coroutine::IsMainCoroutine())
        {
            ErrorLog << "main coroutine can't use coroutine mutex";
            return;
        }

        Mutex::Lock lock(m_mutex);
        if (m_lock)
        {
            m_lock = false;
            if (m_sleep_cors.empty())
            {
                return;
            }

            Coroutine *cor = m_sleep_cors.front();
            m_sleep_cors.pop();
            lock.unlock();

            if (cor)
            {
                // wakeup the first cor in sleep queue
                DebugLog << "coroutine unlock, now to resume coroutine[" << cor->getCorId() << "]";

                tinyrpc::Reactor::GetReactor()->addTask([cor]()
                                                        { tinyrpc::Coroutine::Resume(cor); }, true);
            }
        }
    }
}