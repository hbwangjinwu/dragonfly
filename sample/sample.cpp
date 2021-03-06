#include "../src/mgr.h"
#include <cstdio>
#include <thread>
#include <chrono>
#include <sstream>
#include <cmath>
#include <iostream>

#include <gperftools/profiler.h>

using namespace df;


const uint32_t kLoopTime = 10000;

struct Input {
    uint64_t ts_;
    std::string str_;

    Input(uint64_t ts, const std::string& str): ts_(ts), str_(str) {} 
};

uint64_t Now() {
    auto now  = std::chrono::high_resolution_clock::now();
    auto nano_time_pt = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    auto epoch = nano_time_pt.time_since_epoch();
    uint64_t now_nano = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();
    return now_nano;
}

void process1(const TaskVec& t, TaskVec& out) {
    out.clear();
    for (const auto& arg : t) {
        auto input_arg = std::static_pointer_cast<Input>(arg);
        uint64_t delta_ns = Now() - input_arg->ts_;
        printf("process1-deltatime:%4.2f(us)\n", delta_ns / 1000.0f);

        uint32_t cnt = 0;
        while (cnt++ < kLoopTime)
        {
            double temp = std::sqrt(cnt);
        }

        static uint32_t cnt1 = 0;
        std::ostringstream os;
        os << "engine-1001 input: " << cnt1++;
        auto tt = std::make_shared<Input>(Now(), os.str());
        out.push_back(std::move(tt));
    }
}

void process2(const TaskVec& t, TaskVec& out) {
    out.clear();
    for (const auto& arg : t) {
        auto input_arg = std::static_pointer_cast<Input>(arg);
        uint64_t delta_ns = Now() - input_arg->ts_;
        printf("process2-deltatime:%4.2f(us)\n", delta_ns / 1000.0f);

        uint32_t cnt = 0;
        while (cnt++ < kLoopTime)
        {
            double temp = std::sqrt(cnt);
        }

        static uint32_t cnt1 = 0;
        std::ostringstream os;
        os << "engine-1002 input: " << cnt1++;
        auto tt = std::make_shared<Input>(Now(), os.str());
        out.push_back(std::move(tt));
    }
}

void process3(const TaskVec& t, TaskVec& out) {
    out.clear();
    for (const auto& arg : t) {
        auto input_arg = std::static_pointer_cast<Input>(arg);
        uint64_t delta_ns = Now() - input_arg->ts_;
        printf("process3-deltatime:%4.2f(us)\n", delta_ns / 1000.0f);

        uint32_t cnt = 0;
        while (cnt++ < kLoopTime)
        {
            double temp = std::sqrt(cnt);
        }

        // static uint32_t cnt1 = 0;
        // std::ostringstream os;
        // os << "engine-1003 input: " << cnt1++;
        // auto tt = std::make_shared<Input>(Now(), os.str());
        // out.push_back(std::move(tt));
    }
}

int main(int argc, char* argv[]) {

    try {
        ProfilerStart("test.prof");

        GraphMgr::Instance()->CreateGraph("sample.conf");
        GraphMgr::Instance()->Dump();
        
        EnginePortID id;
        id.graph_id = 1;
        id.engine_id = 1001;
        id.port_id = 0;

        GraphMgr::Instance()->SetFunctor(id, process1);
        id.engine_id = 1002;

        GraphMgr::Instance()->SetFunctor(id, process2);
        id.engine_id = 1003;

        GraphMgr::Instance()->SetFunctor(id, process3);

        printf("Cpu-num: %d\n", std::thread::hardware_concurrency());

        id.engine_id = 1001;
        uint32_t count = 0;
        for (int cnt =0; cnt < 200; cnt++) {
            std::ostringstream os;
            os << "engine-1000 input: " << count++;
            auto tt = std::make_shared<Input>(Now(), os.str());
            auto t =  std::static_pointer_cast<void>(tt);

            GraphMgr::Instance()->SendData(id, t);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        GraphMgr::Instance()->CleanUp();

        ProfilerStop();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
