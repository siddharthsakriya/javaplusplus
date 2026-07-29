#include "Reporter.hpp"
#include "MethodStats.hpp"
#include "SymbolCache.hpp"
#include "ThreadManager.hpp"
#include "CallTree.hpp"
#include "Logger.hpp"
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <functional>

namespace {
    std::string normalize_class_name(const std::string& signature) {
        if (signature.size() >= 2 && signature.front() == 'L' && signature.back() == ';') {
            std::string name = signature.substr(1, signature.size() - 2);
            std::replace(name.begin(), name.end(), '/', '.');
            return name;
        }
        return signature;
    }

    std::string resolve_name(jvmtiEnv* jvmti, jmethodID method_id) {
        const MethodInfo& info = SymbolCache::instance().get_or_resolve(jvmti, method_id);
        return normalize_class_name(info.class_name) + "::" + info.method_name;
    }

    std::string escape_json(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:   out += c; break;
            }
        }
        return out;
    }
}

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
        oss << "    Self:  " << (stats.self_time_ns / 1000000.0) << " ms\n";
        oss << "    Max Self Time: " << (stats.max_self_time_ns / 1000000.0) << " ms\n";
        oss << "    Min Self Time: " << (stats.min_self_time_ns / 1000000.0) << " ms";
                           
        LOG_INFO(oss.str());
    }

    LOG_INFO("=== End Of Report===");
}

void Reporter::dump_thread_summaries(jvmtiEnv* jvmti, JNIEnv* jni) {
    LOG_INFO("--- Thread Summaries ---");

    for (const auto& [id, summary] : ThreadManager::getInstance().get_thread_summaries()) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "Thread ID=" << summary.id << ", Name=" << summary.name << "\n";
        oss << "    Alive: " << (summary.time_alive_ns / 1000000.0) << " ms (ended)";
        LOG_INFO(oss.str());
    }

    for (const auto& summary : ThreadManager::get_live_thread_summaries(jvmti, jni)) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "Thread ID=" << summary.id << ", Name=" << summary.name << "\n";
        oss << "    Alive: " << (summary.time_alive_ns / 1000000.0) << " ms (still running)";
        LOG_INFO(oss.str());
    }

    LOG_INFO("=== End Of Thread Summaries ===");
}

void Reporter::dump_json(jvmtiEnv* jvmti, JNIEnv* jni, const std::string& path) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        LOG_ERROR("Failed to open JSON output file: " + path);
        return;
    }

    const auto& stats_map = MethodStatsRegistry::getInstance().get_stats();

    std::ostringstream oss;
    oss << "{\n";

    oss << "  \"methods\": [\n";
    bool first = true;
    for (const auto& [method_id, stats] : stats_map) {
        const MethodInfo& info = SymbolCache::instance().get_or_resolve(jvmti, method_id);

        if (!first) oss << ",\n";
        first = false;

        oss << "    {\n";
        oss << "      \"class\": \"" << escape_json(info.class_name) << "\",\n";
        oss << "      \"method\": \"" << escape_json(info.method_name) << "\",\n";
        oss << "      \"signature\": \"" << escape_json(info.signature) << "\",\n";
        oss << "      \"call_count\": " << stats.call_count << ",\n";
        oss << "      \"total_time_ns\": " << stats.total_time_ns << ",\n";
        oss << "      \"self_time_ns\": " << stats.self_time_ns << ",\n";
        oss << "      \"min_self_time_ns\": " << stats.min_self_time_ns << ",\n";
        oss << "      \"max_self_time_ns\": " << stats.max_self_time_ns << "\n";
        oss << "    }";
    }
    oss << "\n  ],\n";

    auto write_thread = [&](const ThreadSummary& summary, bool still_running) {
        if (!first) oss << ",\n";
        first = false;

        oss << "    {\n";
        oss << "      \"id\": " << summary.id << ",\n";
        oss << "      \"name\": \"" << escape_json(summary.name) << "\",\n";
        oss << "      \"start_time_ns\": " << summary.start_time_ns << ",\n";
        if (still_running) {
            oss << "      \"end_time_ns\": null,\n";
        } else {
            oss << "      \"end_time_ns\": " << summary.end_time_ns << ",\n";
        }
        oss << "      \"time_alive_ns\": " << summary.time_alive_ns << ",\n";
        oss << "      \"still_running\": " << (still_running ? "true" : "false") << "\n";
        oss << "    }";
    };

    oss << "  \"threads\": [\n";
    first = true;
    for (const auto& [id, summary] : ThreadManager::getInstance().get_thread_summaries()) {
        write_thread(summary, false);
    }
    for (const auto& summary : ThreadManager::get_live_thread_summaries(jvmti, jni)) {
        write_thread(summary, true);
    }
    oss << "\n  ]\n";

    oss << "}\n";

    out << oss.str();
    out.close();

    LOG_INFO("JSON report written to: " + path);
}

void Reporter::dump_call_tree(jvmtiEnv* jvmti) {
    LOG_INFO("--- Call Tree ---");

    const CallTreeNode& root = CallTreeRegistry::getInstance().root();

    int64_t total_ns = 0;
    for (const auto& [method_id, child] : root.children) {
        total_ns += child->inclusive_time_ns;
    }

    if (total_ns == 0) {
        LOG_INFO("(no samples recorded)");
        LOG_INFO("=== End Of Call Tree ===");
        return;
    }

    std::function<void(const CallTreeNode&, int)> visit = [&](const CallTreeNode& node, int depth) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << std::string(depth * 2, ' ');
        oss << resolve_name(jvmti, node.method_id);
        oss << " (count=" << node.sample_count;
        oss << ", inclusive=" << (100.0 * node.inclusive_time_ns / total_ns) << "%";
        oss << ", exclusive=" << (100.0 * node.self_time_ns / total_ns) << "%)";
        LOG_INFO(oss.str());

        std::vector<const CallTreeNode*> children;
        for (const auto& [method_id, child] : node.children) {
            children.push_back(child.get());
        }
        std::sort(children.begin(), children.end(), [](const CallTreeNode* a, const CallTreeNode* b) {
            return a->inclusive_time_ns > b->inclusive_time_ns;
        });
        for (const CallTreeNode* child : children) {
            visit(*child, depth + 1);
        }
    };

    std::vector<const CallTreeNode*> roots;
    for (const auto& [method_id, child] : root.children) {
        roots.push_back(child.get());
    }
    std::sort(roots.begin(), roots.end(), [](const CallTreeNode* a, const CallTreeNode* b) {
        return a->inclusive_time_ns > b->inclusive_time_ns;
    });
    for (const CallTreeNode* r : roots) {
        visit(*r, 0);
    }

    LOG_INFO("=== End Of Call Tree ===");
}

void Reporter::dump_folded_stacks(jvmtiEnv* jvmti, const std::string& path) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        LOG_ERROR("Failed to open folded-stack output file: " + path);
        return;
    }

    const CallTreeNode& root = CallTreeRegistry::getInstance().root();

    std::function<void(const CallTreeNode&, const std::string&)> visit =
        [&](const CallTreeNode& node, const std::string& prefix) {
            std::string path_str = prefix.empty()
                ? resolve_name(jvmti, node.method_id)
                : prefix + ";" + resolve_name(jvmti, node.method_id);

            if (node.self_count > 0) {
                out << path_str << " " << node.self_count << "\n";
            }
            for (const auto& [method_id, child] : node.children) {
                visit(*child, path_str);
            }
        };

    for (const auto& [method_id, child] : root.children) {
        visit(*child, "");
    }

    out.close();
    LOG_INFO("Folded-stack report written to: " + path);
}