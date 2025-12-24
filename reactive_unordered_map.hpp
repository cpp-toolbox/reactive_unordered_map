#ifndef UNORDERED_MAP_WRAPPER_HPP
#define UNORDERED_MAP_WRAPPER_HPP

#include <unordered_map>
#include <optional>
#include <utility>
#include <stdexcept>

#include "sbpt_generated_includes.hpp"

//
// Signals for UnorderedMap modification events
//
template <typename Key, typename Value> struct MapSignals {
    // Fired when a new key-value pair is inserted
    struct Inserted {
        const Key &key;
        const Value &value;

	Inserted(const Key &key, const Value &value) : key(key), value(value) {};
    };

    // Fired when an existing key's value changes
    struct Updated {
        const Key &key;
        const Value &old_value;
        const Value &new_value;
    };

    // Fired when an element is erased
    struct Erased {
        const Key &key;
        const Value &old_value;
    };

    // Fired when the entire map is cleared
    struct Cleared {};

    // Optional: fired when internal bucket structure changes
    struct Rehashed {
        std::size_t old_bucket_count;
        std::size_t new_bucket_count;
    };

    // Optional: fired when reserve() changes capacity
    struct Reserved {
        std::size_t new_capacity;
    };
};

/**
 * @class ReactiveUnorderedMap
 * @brief A wrapper around std::unordered_map that provides reactive signals for insertions and deletions.
 *
 * @warn not all signals are being emitted in all functions, this is being developed on an "add as you need" basis
 *
 * This class behaves like a standard unordered_map with additional capabilities:
 * 1. **Reactive Signals**: Automatically emits signals when elements are inserted or erased.
 *    - `inserted_signal`: Emitted after a successful insertion (via `emplace`).
 *    - `erased_signal`: Emitted after a successful erase.
 * 2. **Standard Map Operations**: Supports most operations of `std::unordered_map`, including:
 *    - Lookup (`find`, `at`, `contains`, `operator[]`)
 *    - Iteration (`begin`, `end`)
 *    - Capacity queries (`empty`, `size`, `reserve`)
 *    - Modifiers (`insert_or_assign`, `try_emplace`, `emplace`, `erase`, `clear`, `update_if_exists`)
 *
 * @tparam Key   Type of the map's keys.
 * @tparam Value Type of the map's values.
 * @tparam Hash  Hash function used for keys. Defaults to std::hash<Key>.
 * @tparam KeyEq Key equality comparator. Defaults to std::equal_to<Key>.
 *
 * @note Signals are emitted through an internal `SignalEmitter`. Connecting to these signals allows
 *       external code to react to map changes without directly modifying the map.
 *
 * @example
 * ReactiveUnorderedMap<int, std::string> my_map;
 * my_map.signal_emitter.connect<ReactiveUnorderedMap<int, std::string>::inserted_signal>(
 *     [](const auto &sig){ std::cout << "Inserted: " << sig.key << " -> " << sig.value << "\n"; });
 *
 * my_map.emplace(1, "hello"); // Emits an inserted signal
 * my_map.erase(1);             // Emits an erased signal
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEq = std::equal_to<Key>>
class ReactiveUnorderedMap {
  public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = std::size_t;
    using map_type = std::unordered_map<Key, Value, Hash, KeyEq>;
    using iterator = typename map_type::iterator;
    using const_iterator = typename map_type::const_iterator;

    // startfold specific signals
    using map_signals = MapSignals<Key, Value>;
    using inserted_signal = typename map_signals::Inserted;
    using erased_signal = typename map_signals::Erased;
    // endfold

    ReactiveUnorderedMap() = default;

    SignalEmitter signal_emitter;

    // ---------- Observers ----------
    bool empty() const noexcept { return map_.empty(); }
    size_type size() const noexcept { return map_.size(); }
    void reserve(size_type n) { map_.reserve(n); }

    iterator begin() noexcept { return map_.begin(); }
    iterator end() noexcept { return map_.end(); }
    const_iterator begin() const noexcept { return map_.begin(); }
    const_iterator end() const noexcept { return map_.end(); }

    // ---------- Lookup ----------
    iterator find(const Key &k) { return map_.find(k); }
    const_iterator find(const Key &k) const { return map_.find(k); }
    bool contains(const Key &k) const { return map_.find(k) != map_.end(); }

    Value &at(const Key &k) { return map_.at(k); }
    const Value &at(const Key &k) const { return map_.at(k); }

    // ---------- Modifiers ----------
    std::pair<iterator, bool> insert_or_assign(const Key &key, Value &&value) {
        return map_.insert_or_assign(key, std::move(value));
    }

    template <typename... Args> std::pair<iterator, bool> try_emplace(const Key &key, Args &&...args) {
        return map_.try_emplace(key, std::forward<Args>(args)...);
    }

    /**
     * @brief runs the internal call and emits an inserted signal if succeeded
     */
    template <class... Args> std::pair<iterator, bool> emplace(Args &&...args) {
        auto [it, inserted] = map_.emplace(std::forward<Args>(args)...);
        if (inserted)
            signal_emitter.emit(typename map_signals::Inserted(it->first, it->second));
        return {it, inserted};
    }

    Value &operator[](const Key &key) { return map_[key]; }

    /**
     * @brief runs the internal call and emits an erased signal if succeeded
     */
    size_type erase(const Key &key) {

        auto it = map_.find(key);
        if (it == map_.end())
            return 0;

        Value old_value = it->second; // capture before erasing
        size_type num_elts_erased = map_.erase(key);

        if (num_elts_erased == 1)
            signal_emitter.emit(typename map_signals::Erased{key, old_value});

        return num_elts_erased;
    }

    iterator erase(iterator pos) { return map_.erase(pos); }

    void clear() noexcept { map_.clear(); }

    bool update_if_exists(const Key &key, Value &&new_value) {
        auto it = map_.find(key);
        if (it == map_.end())
            return false;
        it->second = std::move(new_value);
        return true;
    }

  private:
    map_type map_;
};

#endif // UNORDERED_MAP_WRAPPER_HPP
