#ifndef UTIL_HPP
#define UTIL_HPP

#include <unordered_map>
#include <unordered_set>

namespace Util {
    template <typename K, typename V>
    std::unordered_set<K> getKeys(const std::unordered_map<K, V>& map) 
    {
        std::unordered_set<K> allKeys;
        allKeys.reserve(map.size());

        for(const auto& [key, _]: map){
            allKeys.insert(key);
        }

        return allKeys;
    }
};

#endif
