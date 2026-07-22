#include "Reporter.hpp"
#include "MethodStats.hpp"
#include "SymbolCache.hpp"
#include "Logger.hpp"
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>

void Reporter::dump_report(jvmtiEnv* jvmti) {
    LOG_INFO("=== Profiling Report ===");

    const auto& stats_map = MethodStatsRegistry::getInstance().get_stats();

    std::vector<std::pair<jmethodID, MethodStats>> sorted_stats (stats_map.begin(), stats_map.end());

    std::sort(sorted_stats.begin(), sorted_stats.end(), [](const auto& a, const auto& b) {
        return a.second.total_time_ns > b.second.total_time_ns;
    });

        LOG_INFO("--- Top 20 Methods by Total Time ---");
    int count = 0;
    for (const auto& [method_id, stats] : sorted_stats) {
        if (count++ >= 20) break; 
        
        const MethodInfo& info = SymbolCache::instance().get_or_resolve(jvmti, method_id);    
    
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << info.class_name << " :: " << info.method_name << "\n";
        oss << "    Calls: " << stats.call_count << "\n";
        oss << "    Total: " << (stats.total_time_ns / 1000000.0) << " ms\n";
        oss << "    Self:  " << (stats.self_time_ns / 1000000.0) << " ms";
                           
        LOG_INFO(oss.str());
    }

    LOG_INFO("=== End Of Report===");
}