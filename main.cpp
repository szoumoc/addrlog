#include <iostream>
#include <algorithm>
#include <random>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>
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
};

class Logger
{
public:
    void logAccess(AccessType type, void* address, size_t size)
    {
        size_t slot = LOG_INDEX++;
        if(slot >= MAX_LOGS)
        {
            return;
        }
        buffer[slot] = {type, address, size};
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
private:
    static const size_t MAX_LOGS = 1000000;
    AccessLog buffer[MAX_LOGS];
    size_t LOG_INDEX = 0;
};

thread_local Logger logger;

template <typename T>
T loggedRead(T* address){
    logger.logAccess(AccessType::READ, address, sizeof(T));
    return *address;
}

template <typename T>
void loggedWrite(T* address, T value){
    logger.logAccess(AccessType::WRITE, address, sizeof(T));
    *address = value;
}

void printStrideStats(const std::vector<int>& strides, const std::string& label) {
    if (strides.empty()) {
        std::cout << label << ": No strides to analyze\n";
        return;
    }

    std::map<int, int> freq;
    long long sum = 0;
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

    std::cout << "  Most common strides:\n";
    int count = 0;
    for (const auto& pair : freq) {
        if (count++ >= 5) break;
        double percentage = 100.0 * pair.second / strides.size();
        std::cout << "    " << pair.first << " bytes: " << pair.second
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
    std::cout << "Total log entries: " << logs.size() << "\n";

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

    const int CACHE_LINE_BITS = 7;
    int read_cache_changes = 0;
    int write_cache_changes = 0;


    std::vector<int> read_strides;
    for (size_t i = 1; i < read_addrs.size(); ++i) {
        int stride = static_cast<int>(read_addrs[i] - read_addrs[i-1]);
        read_strides.push_back(stride);
        if ((read_addrs[i] >> CACHE_LINE_BITS) != (read_addrs[i-1] >> CACHE_LINE_BITS)) {
            read_cache_changes++;
        }
    }
    printStrideStats(read_strides, "READ");
    std::cout << "  READ cache line changes: " << read_cache_changes
              << " (" << (100.0 * read_cache_changes / read_strides.size()) << "%)\n";


    std::vector<int> write_strides;
    for (size_t i = 1; i < write_addrs.size(); ++i) {
        int stride = static_cast<int>(write_addrs[i] - write_addrs[i-1]);
        write_strides.push_back(stride);
        if ((write_addrs[i] >> CACHE_LINE_BITS) != (write_addrs[i-1] >> CACHE_LINE_BITS)) {
            write_cache_changes++;
        }
    }
    printStrideStats(write_strides, "WRITE");
    std::cout << "  WRITE cache line changes: " << write_cache_changes
              << " (" << (100.0 * write_cache_changes / write_strides.size()) << "%)\n";
}


void funcTestSequential(int* data, int N) {
    std::cout << "Thread " << std::this_thread::get_id() << ": Starting SEQUENTIAL test (N=" << N << ")\n";
    logger.resetIndex();

    const int REPEATS = 1;

    for (int r = 0; r < REPEATS; ++r) {
        for (int i = 0; i < N; ++i) {
            loggedRead(&data[i]);
            loggedWrite(&data[i], i);
        }
    }

    {
        std::unique_lock<std::mutex> lock(pmtx);
        analyzeStrides();
        std::cout << "Thread " << std::this_thread::get_id() << ": log checksum = " << logger.checksum() << '\n';
    }
}


void funcTestRandom(int* data, int* indices, int N) {
    std::cout << "Thread " << std::this_thread::get_id() << ": Starting RANDOM test (N=" << N << ")\n";
    logger.resetIndex();

    const int REPEATS = 1;

    for (int r = 0; r < REPEATS; ++r) {
        for (int i = 0; i < N; ++i) {
            int idx = indices[i];
            loggedRead(&data[idx]);
            loggedWrite(&data[idx], i);
        }
    }

    {
        std::unique_lock<std::mutex> lock(pmtx);
        analyzeStrides();
        std::cout << "Thread " << std::this_thread::get_id() << ": log checksum = " << logger.checksum() << '\n';
    }
}

int main() {


    const int N = 10000;

    int* data = new int[N];
    int* data2 = new int[N];
    int* indices = new int[N];


    for(int i = 0; i < N; ++i) {
        data[i] = i;
        data2[i] = i;
        indices[i] = i;
    }


    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices, indices + N, gen);

    {

        std::jthread t1(funcTestSequential, data, N);
        std::jthread t2(funcTestRandom, data2, indices, N);
    }

    delete[] data;
    delete[] data2;
    delete[] indices;

    return 0;
}
