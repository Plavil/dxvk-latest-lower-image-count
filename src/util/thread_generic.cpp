#include "thread_generic.h"

#include "./log/log.h"

namespace dxvk {

    ThreadFn::ThreadFn(Proc&& proc)
    : m_proc(std::move(proc)) {
      // Reference for the thread function
      this->incRef();

      pthread_create(&m_handle, NULL, ThreadFn::threadProc, this);

      if(m_handle == 0)
        throw DxvkError("Failed to create thread");
    }

    ThreadFn::~ThreadFn() {
      if (this->joinable())
        std::terminate();
    }

    void ThreadFn::join() {
      if(pthread_join(m_handle, NULL))
        throw DxvkError("Failed to join thread");
      this->detach();
    }

    bool ThreadFn::joinable() const {
        return m_handle != 0;
    }

    void ThreadFn::detach() {
      pthread_detach(m_handle);
      m_handle = 0;
    }

    void ThreadFn::set_priority(ThreadPriority priority)
    {
      // Based on wine staging server-Realtime_Priority patch

      struct sched_param param;
      int policy = SCHED_OTHER;

      switch (priority) {
        case ThreadPriority::Highest:
          policy = SCHED_FIFO;
          param.sched_priority = 2;
          break;

        case ThreadPriority::High:
          policy = SCHED_FIFO;
          param.sched_priority = 0;
          break;

        case ThreadPriority::Normal:
          policy = SCHED_OTHER;
          break;

        case ThreadPriority::Low:
          policy = SCHED_IDLE;
          break;

        case ThreadPriority::Lowest:
          policy = SCHED_BATCH;
          break;
      }

      if (pthread_setschedparam(pthread_self(), policy, &param) == -1)
        Logger::warn("Failed to set thread priority");
    }

    void* ThreadFn::threadProc(void *arg) {
      auto thread = reinterpret_cast<ThreadFn*>(arg);
      thread->m_proc();
      thread->decRef();
      return nullptr;
    }

}