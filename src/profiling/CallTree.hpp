// src/profiling/CallTree.hpp
#pragma once
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <jvmti.h>

struct CallTreeNode {
    jmethodID method_id = nullptr;
    int64_t inclusive_time_ns = 0; // time attributed while this node was anywhere on a sampled stack
    int64_t self_time_ns = 0;      // time attributed while this node was the top-of-stack 
    int sample_count = 0;
    int self_count = 0;
    std::unordered_map<jmethodID, std::unique_ptr<CallTreeNode>> children;
};

class CallTreeRegistry {
    public:
        static CallTreeRegistry& getInstance() {
            static CallTreeRegistry registry;
            return registry;
        }

        void record_stack(const jvmtiFrameInfo* frames, jint frame_count, int64_t delta_ns) {
            if (frame_count <= 0) return;

            std::lock_guard<std::mutex> lock(tree_mutex);
            CallTreeNode* current = &root_node;

            for (jint i = frame_count - 1; i >= 0; --i) {
                std::unique_ptr<CallTreeNode>& child = current->children[frames[i].method];
                if (!child) {
                    child = std::make_unique<CallTreeNode>();
                    child->method_id = frames[i].method;
                }
                child->inclusive_time_ns += delta_ns;
                child->sample_count++;
                if (i == 0) {
                    child->self_time_ns += delta_ns;
                    child->self_count++;
                }
                current = child.get();
            }
        }

        const CallTreeNode& root() const {
            return root_node;
        }

    private:
        CallTreeRegistry() = default;
        CallTreeNode root_node;
        std::mutex tree_mutex;
};
