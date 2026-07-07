

// class Logger
// {
// public:
//     void logAccess(AccessType type, void* address, size_t size)
//     {
//         AccessLog log{type, address, size};
//         buffer[LOG_INDEX] = log;
//         LOG_INDEX++;
//     }

//     void dumpLogs()
//     {
//         if(LOG_INDEX>MAX_LOGS)
//         {
//             return;
//         }
        
//         for (size_t i = 0; i < LOG_INDEX; ++i)
//         {
//             const AccessLog& log = buffer[i];
//             std::cout << "Access Type: " << (log.type == AccessType::READ ? "READ" : "WRITE")
//                       << ", Address: " << log.address
//                       << ", Size: " << log.size << std::endl;
//         }
//     }
// private:
//     static const size_t MAX_LOGS = 100;
//     AccessLog buffer[MAX_LOGS];
//     size_t LOG_INDEX = 0;
// };