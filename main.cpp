#include <iostream>
#include <algorithm>
#include <random>
#include <vector>
#include <numeric>
#include <chrono>
#include<thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

using namespace std;
std::mutex pmtx;
enum class AccessType
{
    READ,
    WRITE
};
struct alignas(128) AccessLog
{
    AccessType type;
    void* address;
    size_t size;
    // std::thread::id threadId;
};


class Logger
{
public:
    void logAccess(AccessType type, void* address, size_t size)
    {
        // size_t slot = LOG_INDEX.fetch_add(1, std::memory_order_relaxed);
        size_t slot = LOG_INDEX++;
        if(slot>=MAX_LOGS)
        {
            // std::cout<<"LOGGER BROKE"<<std::endl;
            return;
        }
        buffer[slot] = {type, address, size};
    }
    void dumpLogs()
    {
        
        for (size_t i = 0; i < LOG_INDEX; ++i)
        {
            const AccessLog& log = buffer[i];
            std::cout << "Access Type: " << (log.type == AccessType::READ ? "READ" : "WRITE")
                      << ", Address: " << log.address
                      << ", Size: " << log.size << std::endl;
            std::cout<<"Size error:"<<(log.size!=sizeof(int))<<std::endl;
        }

    }
    uintptr_t checksum() const
    {
        uintptr_t sum = 0;

        for (size_t i = 0; i < LOG_INDEX; ++i)
        {
            sum += reinterpret_cast<uintptr_t>(buffer[i].address);
            sum += buffer[i].size;
            sum += static_cast<uintptr_t>(buffer[i].type);
        }

        return sum;
    }
    void resetIndex(){
        LOG_INDEX = 0;
    }
    size_t getIndex() const{
        return LOG_INDEX;
    }
private:
    static const size_t MAX_LOGS = 100000;
    AccessLog buffer[MAX_LOGS];
    // std::atomic<size_t> LOG_INDEX{0};
    size_t LOG_INDEX = 0;

};

thread_local Logger logger;

template <typename T>
T loggedRead(T* address){
    // std::cout << "Reading from address: " << address << std::endl;
    logger.logAccess(AccessType::READ, address, sizeof(T));
    return *address;
}

template <typename T>
void loggedWrite(T* address, T value){
    // std::cout << "Writing to address: " << address << " with value: " << value << std::endl;
    logger.logAccess(AccessType::WRITE, address, sizeof(T));
    *address = value;
}


// void loggerReset(Logger& logger){
//     logger.resetIndex();
// }


void funcTest(int* data){
    logger.resetIndex();
    const int REPEATS = 1'000;
    const int N       = 32;
        for (int r = 0; r < REPEATS; ++r){
            for (int i = 0; i < N; ++i) {
                // std::cout<<"thread id:"<<std::this_thread::get_id()<<" r:"<<r<<" i:"<<i<<std::endl;
                loggedRead(&data[i]);
                loggedWrite(&data[i], i);
            }
        }
        {
            std::unique_lock<std::mutex> lock(pmtx);
            logger.dumpLogs();
            std::cout<<"thread id:"<<std::this_thread::get_id()<<" log checksum  = " << logger.checksum() << '\n';
            // lock.unlock();
        }
}



int main() {

    const int REPEATS = 1'000;
    const int N       = 32;
    int data[N];
    int data2[N];
    {
        // auto logger = std::make_unique<Logger>();
        std::jthread t1(funcTest,data);
        std::jthread t2(funcTest,data2);
    }


    // {
    //     Logger logger;
    //     long long sum = 0;

    //     auto startA = std::chrono::high_resolution_clock::now();

    //     for (int r = 0; r < REPEATS; ++r)
    //     {
    //         for (int i = 0; i < N; ++i)
    //         {
    //             loggedRead(&data[i],logger);
    //             loggedWrite(&data[i], i,logger);
    //         }
    //     }

    //     auto endA = std::chrono::high_resolution_clock::now();

    //     long long totalA =
    //         std::chrono::duration_cast<std::chrono::nanoseconds>(endA - startA).count();

    //     std::cout << "Variant A (logged):\n";
    //     std::cout << "  total      = " << totalA << " ns\n";
    //     // std::cout << "  per-access = "
    //     //         << totalA / (2LL * N * REPEATS)
    //     //         << " ns\n";

    //     // std::cout << "Read checksum = " << sum << '\n';
    //     std::cout << "Log checksum  = " << logger.checksum() << '\n';
    // }
    // {
    //     // volatile long long dummy = 0;
    //     volatile long long sum = 0;

    //     auto startB = std::chrono::high_resolution_clock::now();

    //     for (int r = 0; r < REPEATS; ++r){
    //         for (int i = 0; i < N; ++i) {
    //             data[i] = i;
    //             sum += data[i];
    //         }
    //     }

    //     auto endB = std::chrono::high_resolution_clock::now();
    //     // dummy = sum;

    //     long long totalB =
    //         std::chrono::duration_cast<std::chrono::nanoseconds>(endB - startB).count();

    //     std::cout << "Variant B (raw)    :\n"
    //               << "total = " << totalB << " ns\n"
    //               << "sum=" << sum<<"\n";
    // }
    // std::cout<<"size:"<<sizeof(AccessLog);
    return 0;
}  