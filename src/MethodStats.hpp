// src/MethodStats.hpp
#pragma once
#include <jvmti.h>
#include <cstdint>
#include <mutex>
#include <unordered_map>

struct MethodStats {
    int call_count = 0;
    int64_t total_time_ns = 0; //inclusive time (inc methods called)
    int64_t total_time_ns = 0; //exclusive time  (exec time of curr method)
}

class MethodStatsRegistry {
    public:
        static MethodStatsRegistry& getInstance() {
            static MethodStatsRegistry registry;
            return registry;
        }

        void add_stats(jmethodID method_id, int64_t inclusive_ns, int64_t exclusive_ns) {
            std::lock_guard<std::mutex> lock(stats_mutex);
            MethodStats& stats = stats_map[method_id];
            stats.call_count++;
            stats.total_time_ns += inclusive_ns;
            stats.self_time_ns += exclusive_ns;
        }

        const std::unordered_map<jmethodID, MethodStats>& get_stats() const {
        return stats_map;
        }   
    
    private:
        MethodStatsRegistry() = default;
        std::unordered_map<jmethodID, MethodStats> stats_map;
        std::mutex stats_mutex;
}

