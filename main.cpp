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
#include <map>

using namespace std;
std::mutex pmtx;
enum class AccessType
{
    READ,
    WRITE
};
struct AccessLog
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
    void getLogs(std::vector<AccessLog>& logs) const
    {
        logs.clear();
        logs.reserve(LOG_INDEX);
        for (size_t i = 0; i < LOG_INDEX; ++i)
        {
            logs.push_back(buffer[i]);
        }
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



void printStrideStats(const std::vector<int>& strides, const std::string& label) {
    if (strides.empty()) {
        std::cout << label << ": No strides to analyze\n";
        return;
    }
    
    // Count frequency of each stride
    std::map<int, int> freq;
    int sum = 0;
    int zero_count = 0;
    int max_stride = INT_MIN;
    int min_stride = INT_MAX;
    
    for (int s : strides) {
        freq[s]++;
        sum += s;
        if (s == 0) zero_count++;
        max_stride = std::max(max_stride, s);
        min_stride = std::min(min_stride, s);
    }
    
    double avg = static_cast<double>(sum) / strides.size();
    std::cout << label << " strides:\n";
    std::cout << "  Total: " << strides.size() << "\n";
    std::cout << "  Sum: " << sum << " bytes\n";
    std::cout << "  Average: " << avg << " bytes\n";
    std::cout << "  Min: " << min_stride << " bytes\n";
    std::cout << "  Max: " << max_stride << " bytes\n";
    std::cout << "  Zero strides: " << zero_count 
              << " (" << (100.0 * zero_count / strides.size()) << "%)\n";
    
    // Show top 5 most common strides
    std::cout << "  Most common strides:\n";
    int count = 0;
    for (const auto& [stride, count_] : freq) {
        if (count++ >= 5) break;
        double percentage = 100.0 * count_ / strides.size();
        std::cout << "    " << stride << " bytes: " << count_ 
                  << " (" << percentage << "%)\n";
    }
}


void analyzeStrides() {
    std::vector<AccessLog> logs;
    logger.getLogs(logs);
    
    if (logs.size() < 2) {
        std::cout << "Not enough logs for stride analysis\n";
        return;
    }
    
    std::cout << "\n=== OPTION B: Type-Separated Strides ===\n";
    
    std::vector<uintptr_t> read_addrs;
    std::vector<uintptr_t> write_addrs;
    
    for (const auto& log : logs) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(log.address);
        if (log.type == AccessType::READ) {
            read_addrs.push_back(addr);
        } else {
            write_addrs.push_back(addr);
        }
    }
    int cache_line_changes = 0;
    // Analyze READ strides
    std::vector<int> read_strides;
    for (size_t i = 1; i < read_addrs.size(); ++i) {
        int stride = static_cast<int>(read_addrs[i] - read_addrs[i-1]);
        read_strides.push_back(stride);
        //cache line analysis
        if(read_addrs[i]/128 != read_addrs[i-1]/128){
            ++cache_line_changes;
        }

    }
    printStrideStats(read_strides, "READ");
    
    // Analyze WRITE strides
    std::vector<int> write_strides;
    for (size_t i = 1; i < write_addrs.size(); ++i) {
        int stride = static_cast<int>(write_addrs[i] - write_addrs[i-1]);
        write_strides.push_back(stride);
        //cache line analysis
        if(write_addrs[i]/128 != write_addrs[i-1]/128){
            ++cache_line_changes;
        }
    }
    printStrideStats(write_strides, "WRITE");
    std::cout << "Total cache line changes: " << cache_line_changes << std::endl;
}




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
            // logger.dumpLogs();
            analyzeStrides();
            std::cout<<"thread id:"<<std::this_thread::get_id()<<" log checksum  = " << logger.checksum() << '\n';
            // lock.unlock();
        }
}


void funcTest2(int* data, int* indices){
    std::cout<<"thread id:"<<std::this_thread::get_id()<<" start funcTest2"<<std::endl;
    logger.resetIndex();
    const int REPEATS = 1'000;
    const int N       = 32;
        for (int r = 0; r < REPEATS; ++r){
            for (int i = 0; i < N; ++i) {
                // std::cout<<"thread id:"<<std::this_thread::get_id()<<" r:"<<r<<" i:"<<i<<std::endl;
                // data[indices[i]] = i;
                loggedRead(&data[indices[i]]);
                loggedWrite(&data[indices[i]], i);
            }
        }
        {
            std::unique_lock<std::mutex> lock(pmtx);
            // logger.dumpLogs();
            analyzeStrides();
            std::cout<<"thread id:"<<std::this_thread::get_id()<<" log checksum  = " << logger.checksum() << '\n';
            // lock.unlock();
        }
}


int main() {

    const int REPEATS = 1'000;
    const int N       = 32;
    int data[N];
    int data2[N];
    int indices[N];
    for(int i = 0; i < N; ++i) indices[i] = i;  // Initialize first!

    // Then shuffle
    for(int i = 31; i >= 0; --i){
        int j = rand() % (i + 1);
        std::swap(indices[i], indices[j]);
    }

    {
        // auto logger = std::make_unique<Logger>();
        // std::jthread t1(funcTest,data);
        // std::jthread t2(funcTest,data2);
        std::jthread t3(funcTest2,data, indices);
        std::jthread t4(funcTest2,data2, indices);
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