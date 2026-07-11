#include "memtrace_rt.h"
#include "memtrace_html.h"
#include <iostream>
#include <vector>
#include <map>
#include <climits>
#include <cstdio>
#include <cstdint>

enum class AccessType {
    READ = 0,
    WRITE = 1
};

struct AccessLog {
    AccessType type;
    void* address;
    size_t size;
};

class Logger {
public:
    void logAccess(AccessType type, void* address, size_t size) {
        size_t slot = LOG_INDEX++;
        if (slot >= MAX_LOGS) return;
        buffer[slot] = {type, address, size};
    }

    void getLogs(std::vector<AccessLog>& logs) const {
        logs.clear();
        logs.reserve(LOG_INDEX);
        for (size_t i = 0; i < LOG_INDEX; ++i)
            logs.push_back(buffer[i]);
    }

    void resetIndex() { LOG_INDEX = 0; }
    size_t getIndex() const { return LOG_INDEX; }

    uintptr_t checksum() const {
        uintptr_t sum = 0;
        for (size_t i = 0; i < LOG_INDEX; ++i) {
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



extern "C" void logAccess(void* address, size_t size, int type) {
    logger.logAccess(static_cast<AccessType>(type), address, size);
}

static void printStrideStats(const std::vector<int>& strides, const std::string& label) {
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
    std::cout << "  Total: "   << strides.size() << "\n";
    std::cout << "  Sum: "     << sum             << " bytes\n";
    std::cout << "  Average: " << avg             << " bytes\n";
    std::cout << "  Min: "     << min_stride      << " bytes\n";
    std::cout << "  Max: "     << max_stride      << " bytes\n";
    std::cout << "  Zero strides: " << zero_count
              << " (" << (100.0 * zero_count / strides.size()) << "%)\n";

    std::cout << "  Most common strides:\n";
    int count = 0;
    for (const auto& [stride, cnt] : freq) {
        if (count++ >= 5) break;
        std::cout << "    " << stride << " bytes: " << cnt
                  << " (" << (100.0 * cnt / strides.size()) << "%)\n";
    }
}




extern "C" void analyzeAndPrint() {
    std::vector<AccessLog> logs;
    logger.getLogs(logs);

    if (logs.size() < 2) {
        std::cout << "Not enough logs for stride analysis\n";
        return;
    }

    std::cout << "\n=== MEMTRACE: Type-Separated Strides ===\n";
    std::cout << "Total log entries: " << logs.size() << "\n";

    std::vector<uintptr_t> read_addrs, write_addrs;
    for (const auto& log : logs) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(log.address);
        if (log.type == AccessType::READ)
            read_addrs.push_back(addr);
        else
            write_addrs.push_back(addr);
    }

    const int CACHE_LINE_BITS = 7;


    std::vector<int> read_strides;
    int read_cache_changes = 0;
    for (size_t i = 1; i < read_addrs.size(); ++i) {
        read_strides.push_back(static_cast<int>(read_addrs[i] - read_addrs[i-1]));
        if ((read_addrs[i] >> CACHE_LINE_BITS) != (read_addrs[i-1] >> CACHE_LINE_BITS))
            read_cache_changes++;
    }
    printStrideStats(read_strides, "READ");
    if (!read_strides.empty())
        std::cout << "  READ cache line changes: " << read_cache_changes
                  << " (" << (100.0 * read_cache_changes / read_strides.size()) << "%)\n";


    std::vector<int> write_strides;
    int write_cache_changes = 0;
    for (size_t i = 1; i < write_addrs.size(); ++i) {
        write_strides.push_back(static_cast<int>(write_addrs[i] - write_addrs[i-1]));
        if ((write_addrs[i] >> CACHE_LINE_BITS) != (write_addrs[i-1] >> CACHE_LINE_BITS))
            write_cache_changes++;
    }
    printStrideStats(write_strides, "WRITE");
    if (!write_strides.empty())
        std::cout << "  WRITE cache line changes: " << write_cache_changes
                  << " (" << (100.0 * write_cache_changes / write_strides.size()) << "%)\n";

    std::cout << "Checksum: " << logger.checksum() << "\n";

    // dump self-contained HTML report
    FILE* f = fopen("memtrace_report.html", "w");
    if (f) {
        fprintf(f, "%s", HTML_PREFIX);
        fprintf(f, "[\n");
        for (size_t i = 0; i < logs.size(); ++i) {
            fprintf(f, "  {\"addr\":%lu,\"size\":%zu,\"type\":%d}%s\n",
                reinterpret_cast<uintptr_t>(logs[i].address),
                logs[i].size,
                static_cast<int>(logs[i].type),
                i + 1 < logs.size() ? "," : "");
        }
        fprintf(f, "]\n");
        fprintf(f, "%s", HTML_SUFFIX);
        fclose(f);
        std::cout << "Saved self-contained visualizer to memtrace_report.html\n";
    } else {
        std::cerr << "Failed to open memtrace_report.html for writing\n";
    }
}
