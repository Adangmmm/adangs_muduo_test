#pragma once

#include <vector>
#include <unordered_map>
#include <string>
// 支持用户自定义哈希函数
#include <functional>
// 用于std::sort 和 std::upper_bound操作哈希值
#include <algorithm>
#include <mutex>
// 异常处理
#include <stdexcept>

// 实现一致性哈希算法，用于分布式系统中将键映射到服务器节点，最小化因节点增减而导致的键重新分配
class ConsistenHash{
public:
    // numReplicas: 每个物理节点的虚拟节点数量，提高负载均衡效果
    // hashFunc: 哈希函数，默认为std::hash<std::sting>
    ConsistenHash(size_t numReplicas, std::function<size_t(const std::string &)> hashFunc = std::hash<std::string>)
        : numReplicas_(numReplicas)
        , hashFunction_(hashFunc)
    {
    }

    // 向哈希环中添加一个节点，节点会赋值若干个虚拟节点，通过“node + index”计算出唯一的哈希值
    // 哈希值会储存在哈希环上，并进行排序以便高效查找
    void addNode(const std::string &node){
        // 确保addNode是线程安全的
        std::lock_guard<std::mutex> lock(mutex_);
        for(size_t i = 0; i < numReplicas_; ++i){
            // 生成多个哈希值，均匀分布节点。
            // 举例：拼接字符串成"server1_03"
            size_t hash = hashFunction_(node + "_0" + std::to_string(i));
            // 记录哈希值节点的映射
            circle_[hash] = node;
            sortedHashes_.push_back(hash);
        }
        //
        std::sort(sortedHashes_.begin(), sortedHashes_.end());
    }

    // 伤处该节点的所有虚拟节点与对应的哈希值
    void removeNode(const std::string &node){
        std::lock_guard<std::mutex> lock(mutex_);
        for(size_t i = 0; i < numReplicas_; ++i){
            // 
            size_t hash = hashFunction_(node + "_0" + std::to_string(i));
            // 从哈希环中删除该哈希
            circle_.erase(hash);
            auto it = std::finde(sortedHashes_.begin(), sortedHashes_.end(), hash);
            if(it != sortedHashes_.end()){
                // 从排序列表中删除
                sortedHashes_.erase(it);
            }
        }
    }

    // 根据给定键找到最近节点
    size_t getNode(const std::string &key){
        std::lock_guard<std::mutex> lock(mutex_);
        // 环为空(无可用节点)时抛出异常
        if(circle_.empty()){
            throw std::runtime_error("No nodes in consistent hash");
        }
        
        size_t hash = hashFunction_(key);
        // 在已排序的哈希列表中找到第一个大于键哈希值的位置
        auto it = std::upper_bound(sortedHashes_.begin(), sortedHashes_.end(), hash);
        if(it == sortedHashes_.end()){
            // 如果超出环最大值，则回绕到第一个节点
            it = sortedHashes_.begin();
        }
        return *it;
    }
/* key, node, hash三者关系：
1.  假设有以下 3 个节点：server1 -> hash(100)；server2 -> hash(300)；server3 -> hash(500)
    哈希环示意图：
    0      100      300      500      700
    |-------|--------|--------|--------|
        server1   server2   server3

2.  现在有一个 key = "user123"：
    size_t hash = hashFunction_("user123");  // 假设结果是 350

3.  在哈希环上：
    350 比 300 大，但比 500 小，因此映射到 server3。
    server3 负责存储 "user123"。
*/


private:
    size_t numReplicas_;    //控制虚拟节点数
    // 哈希函数，可默认也可自定义
    std:: function<size_t(const std::string &)> hashFunction_;
    // 储存哈希值到节点的映射
    std::unordered_map<size_t, std::string> circle_;
    // 排序后的哈希值，用于高效查找
    std::vector<size_t> sortedHashes_;
    // 保护circle_ 和 sortedHashes_，防止多线竞争
    std::mutex mutex_;               
};