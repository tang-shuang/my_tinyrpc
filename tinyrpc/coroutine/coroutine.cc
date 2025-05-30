#include "tinyrpc/coroutine/coroutine.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <atomic>

#include "tinyrpc/comm/log.h"
#include "tinyrpc/comm/run_time.h"

namespace tinyrpc
{
    // main coroutine, every io thread have a main_coroutine
    static thread_local Coroutine *t_main_coroutine = NULL;

    // current thread is runing which coroutine
    static thread_local Coroutine *t_cur_coroutine = NULL;

    static thread_local RunTime *t_cur_run_time = NULL;

    static std::atomic_int t_coroutine_count{0};

    static std::atomic_int t_cur_coroutine_id{1};

    int getCoroutineIndex()
    {
        return t_cur_coroutine_id;
    }

    RunTime *getCurrentRunTime()
    {
        return t_cur_run_time;
    }

    void setCurrentRunTime(RunTime *v)
    {
        t_cur_run_time = v;
    }

    void CoFunction(Coroutine *co)
    {

        if (co != nullptr)
        {
            co->setIsInCoFunc(true);

            // 去执行协程回调函数
            co->m_call_back();

            co->setIsInCoFunc(false);
        }

        // here coroutine's callback function finished, that means coroutine's life is over. we should yiled main couroutine
        Coroutine::Yield();
    }

    Coroutine::Coroutine()
    {
        // main coroutine'id is 0
        m_cor_id = 0;
        t_coroutine_count++;
        memset(&m_coctx, 0, sizeof(m_coctx));
        t_cur_coroutine = this;
    }

    Coroutine::Coroutine(int size, char *stack_ptr) : m_stack_size(size), m_stack_sp(stack_ptr)
    {
        assert(stack_ptr);

        if (!t_main_coroutine)
        {
            t_main_coroutine = new Coroutine();
        }

        m_cor_id = t_cur_coroutine_id++;
        t_coroutine_count++;
    }

    Coroutine::Coroutine(int size, char *stack_ptr, std::function<void()> cb)
        : m_stack_size(size), m_stack_sp(stack_ptr)
    {

        assert(m_stack_sp);

        if (!t_main_coroutine)
        {
            t_main_coroutine = new Coroutine();
        }

        setCallBack(cb);
        m_cor_id = t_cur_coroutine_id++;
        t_coroutine_count++;
    }

    Coroutine::~Coroutine()
    {
        t_coroutine_count--;
    }

    bool Coroutine::setCallBack(std::function<void()> cb)
    {
        if (this == t_main_coroutine)
        {
            ErrorLog << "main coroutine can't set callback";
            return false;
        }
        if (m_is_in_cofunc)
        {
            ErrorLog << "this coroutine is in CoFunction";
            return false;
        }

        m_call_back = cb;

        char *top = m_stack_sp + m_stack_size;

        top = reinterpret_cast<char *>((reinterpret_cast<unsigned long>(top)) & -16LL);

        memset(&m_coctx, 0, sizeof(m_coctx));

        m_coctx.regs[kRSP] = top;
        m_coctx.regs[kRBP] = top;
        m_coctx.regs[kRETAddr] = reinterpret_cast<char *>(CoFunction);
        m_coctx.regs[kRDI] = reinterpret_cast<char *>(this);

        m_can_resume = true;

        return true;
    }

    /********
    form target coroutine back to main coroutine
    ********/
    void Coroutine::Yield()
    {
        if (t_main_coroutine == nullptr)
        {
            ErrorLog << "main coroutine is nullptr";
            return;
        }

        if (t_cur_coroutine == t_main_coroutine)
        {
            ErrorLog << "current coroutine is main coroutine";
            return;
        }
        Coroutine *co = t_cur_coroutine;
        t_cur_coroutine = t_main_coroutine;
        t_cur_run_time = NULL;
        coctx_swap(&(co->m_coctx), &(t_main_coroutine->m_coctx));
    }

    /********
    form main coroutine switch to target coroutine
    ********/
    void Coroutine::Resume(Coroutine *co)
    {
        if (t_cur_coroutine != t_main_coroutine)
        {
            ErrorLog << "swap error, current coroutine must be main coroutine";
            return;
        }

        if (!t_main_coroutine)
        {
            ErrorLog << "main coroutine is nullptr";
            return;
        }
        if (!co || !co->m_can_resume)
        {
            ErrorLog << "pending coroutine is nullptr or can_resume is false";
            return;
        }

        if (t_cur_coroutine == co)
        {
            DebugLog << "current coroutine is pending cor, need't swap";
            return;
        }

        t_cur_coroutine = co;
        t_cur_run_time = co->getRunTime();

        coctx_swap(&(t_main_coroutine->m_coctx), &(co->m_coctx));
    }

    Coroutine *Coroutine::GetCurrentCoroutine()
    {
        if (t_cur_coroutine == nullptr)
        {
            t_main_coroutine = new Coroutine();
            t_cur_coroutine = t_main_coroutine;
        }
        return t_cur_coroutine;
    }

    Coroutine *Coroutine::GetMainCoroutine()
    {
        if (t_main_coroutine)
        {
            return t_main_coroutine;
        }
        t_main_coroutine = new Coroutine();
        return t_main_coroutine;
    }

    bool Coroutine::IsMainCoroutine()
    {
        if (t_main_coroutine == nullptr || t_cur_coroutine == t_main_coroutine)
        {
            return true;
        }
        return false;
    }

    int Coroutine::getCorId() const
    {
        return m_cor_id;
    }

    void Coroutine::setIsInCoFunc(const bool v)
    {
        m_is_in_cofunc = v;
    }

    bool Coroutine::getIsInCoFunc() const
    {
        return m_is_in_cofunc;
    }

    std::string Coroutine::getMsgNo()
    {
        return m_msg_no;
    }

    RunTime *Coroutine::getRunTime()
    {
        return &m_run_time;
    }

    void Coroutine::setMsgNo(const std::string &msg_no)
    {
        m_msg_no = msg_no;
    }

    void Coroutine::setIndex(int index)
    {
        m_index = index;
    }

    int Coroutine::getIndex()
    {
        return m_index;
    }

    char *Coroutine::getStackPtr()
    {
        return m_stack_sp;
    }

    int Coroutine::getStackSize()
    {
        return m_stack_size;
    }

    void Coroutine::setCanResume(bool v)
    {
        m_can_resume = v;
    }
}