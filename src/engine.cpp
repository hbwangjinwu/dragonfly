#include "engine.h"
#include "common.h"
#include <cstdio>
#include <cstring>
#include <sched.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <cassert>

namespace df {

    uint64_t Now() {
    auto now  = std::chrono::high_resolution_clock::now();
    auto nano_time_pt = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    auto epoch = nano_time_pt.time_since_epoch();
    uint64_t now_nano = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();
    return now_nano;
}


// Engine::Engine(int id, int priority, const std::string& policy, 
//                const std::string& affi, const std::vector<int>& cpus, 
//                int threadnum, int inputSize):id_(id), priority_(priority), 
//                policy_(policy), cpu_affi_(affi), thread_num_(threadnum),
//                inQueue_(inputSize)
Engine::Engine(int id, int threadnum, int inputSize, int outputSize): inQueue_(inputSize),
    parent_num_(inputSize), child_num_(outputSize), id_(id) {
    childs_.resize(outputSize);
    // parents_.resize(inputSize); 
}
// {
//     cpus_.assign(cpus.begin(), cpus.end());
// }

Engine::~Engine() {
    Stop();
}

void Engine::Init() {
    printf("init thread!!!\n");
    for (int i=0; i<thread_num_; i++) {
        threads_.push_back(std::thread(&Engine::Entry, this));
    }
    // thread_.join();
    // thread_.detach();
}

void Engine::SetFunctor(std::function<TaskVec(const TaskVec&)> f) noexcept {
    functor_ = f;
}

void Engine::Entry() {
    tid_.store(static_cast<int>(syscall(SYS_gettid)));
    for (;;) {
        if (stop_) break;

        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this]{ return !args_.empty();});
        lk.unlock();

if (id_ == 1001) {
    printf("t3: %llu\n", Now());
}
        auto tt = functor_(args_);

        assert(tt.size() == childs_.size());

        for (int i =0; i<tt.size(); i++) {
            childs_[i]->Push(i, tt[i]);
        }
    }
}

void Engine::Stop() noexcept {
    stop_ = true;
    NotifyAll();

    for (auto& thrd: threads_) {
        if (thrd.joinable()) {
            thrd.join();
        }
    }
}

void Engine::NotifyOne() {
    cv_.notify_one();
}

void Engine::NotifyAll() {
    cv_.notify_all();
}

bool Engine::SetChild(int idx, const std::shared_ptr<Engine> ch) noexcept { 
    if (idx >= childs_.size()) return false;    
    childs_[idx] = ch;
    return true;
}

// bool Engine::SetParent(int idx, const std::shared_ptr<Engine> pa) noexcept { 
//     if (idx >= parents_.size()) return false;
//     parents_[idx] = pa;
//     return true;
// }

const std::shared_ptr<Engine> Engine::Child(int idx) const noexcept {
    if (idx <= childs_.size()) return childs_[idx];
    return nullptr;
}

// const std::shared_ptr<Engine> Engine::Parent(int idx) const noexcept { 
//     if (idx <= parents_.size()) return parents_[idx];
//     return nullptr;
// }

void Engine::AddPublisher(uint32_t qidx, const std::string& ip, uint32_t port) noexcept {
    auto ptr = std::make_shared<Publisher>();
    ptr->remote_queue_idx = qidx;
    ptr->remote_ip = ip;
    ptr->remote_port = port;
    publishers_.push_back(std::move(ptr)); 
}

void Engine::AddRecipient(uint32_t qidx, uint32_t port) noexcept {
    auto ptr =std::make_shared<Recipient>();
    ptr->local_queue_idx = qidx;
    ptr->listen_port = port;
    recipients_.push_back(std::move(ptr));
}

bool Engine::SetSchedAffinity(int32_t idx, const std::string& affinity,
                              const std::vector<int>& cpus) {
    if (affinity.empty() || cpus.empty()) return false;
    if (idx >= threads_.size()) return false;

    auto& select_thread = threads_[idx];
    printf("cpus: %d\n", std::thread::hardware_concurrency());
    cpu_set_t set;
    CPU_ZERO(&set);
    if (cpus.size()) {
        printf("####SetSchedAffinity \n");
        if (!affinity.compare("range")) {
            for (const auto cpu : cpus) {
                printf("cpu:%d\n", cpu);
                CPU_SET(cpu, &set);
            }
            pthread_setaffinity_np(select_thread.native_handle(), sizeof(set), &set);
        } else if (!affinity.compare("1to1")) {
            printf("******SetSchedAffinity 1to1 \n");
            CPU_SET(cpus[0], &set);
            pthread_setaffinity_np(select_thread.native_handle(), sizeof(set), &set);
    }
  }
  return true;
}

bool Engine::SetSchedPolicy(int32_t idx, int pority, const std::string& policy) {
    if (policy.empty() || pority == -1) return false;
    if (idx >= threads_.size()) return false;

    auto& select_thread = threads_[idx];
    struct sched_param sp;
    int policyValue;
    memset(reinterpret_cast<void *>(&sp), 0, sizeof(sp));
    sp.sched_priority = pority;

    if (!policy.compare("SCHED_FIFO")) {
        policyValue = SCHED_FIFO;
        pthread_setschedparam(select_thread.native_handle(), policyValue, &sp);
    } else if (!policy.compare("SCHED_RR")) {
        policyValue = SCHED_RR;
        pthread_setschedparam(select_thread.native_handle(), policyValue, &sp);
    } else if (!policy.compare("SCHED_OTHER")) {
        // Set normal thread nice value.
        while (tid_.load() == -1) {
            cpu_relax();
        }
        setpriority(PRIO_PROCESS, tid_.load(), pority);
    }
    return true;
}

// int32_t Engine::Priority() const noexcept {
//     return priority_;
// }

// const std::string& Engine::Policy() const noexcept {
//     return policy_;
// }


// const std::vector<int32_t>& Engine::Cores() const noexcept {
//     return cpus_;
// }

//
struct Input {
    uint64_t ts_;
    std::string str_;

    Input(uint64_t ts, const std::string& str): ts_(ts), str_(str) {} 
};

void Engine::Push(int32_t queueIdx, const Task& data) {
    auto t1=Now();

    printf("++++qidx:%d\n", queueIdx);

    auto input_arg = std::static_pointer_cast<Input>(data);
    printf("@@T: %f\n", (t1 - input_arg->ts_) / 1000.0);

    inQueue_.PushData(queueIdx, data);

    args_.clear();
    if (!inQueue_.PopAllData(args_)) return;

    auto t2 = Now();

    if (id_ == 1001) {
        printf("delta: %f\n", (t2 - t1) / 1000.0);
        printf("t2: %llu\n", t2);
    }

    NotifyOne();
}

// uint32_t Engine::CurrentQueueSize() const noexcept {
//     return queue_.size();
// }

// uint32_t Engine::MaxQueueSize() const noexcept {
//     return 200;
// }

int32_t Engine::Id() const noexcept {
    return id_;
}

void Engine::Dump() const noexcept {
    printf("------\n");
    // printf("engine-id:%d, parent-id:%d, child-id:%d\n", Id(), Parent() == nullptr? 0 : Parent()->Id(), Child() == nullptr? 0 : Child()->Id());
}

}
