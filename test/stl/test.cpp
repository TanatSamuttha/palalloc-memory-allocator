#include <iostream>
#include <unordered_map>
#include <cassert>

#include "palalloc.h"
#include "palalloc_wrapper.h"

int main() {
    using KeyType = int;
    using ValueType = int;
    using PairType = std::pair<const KeyType, ValueType>;

    Palalloc palalloc(4, 64);
    PalallocWrapper<PairType> wrapper(palalloc);

    std::unordered_map<
        KeyType,
        ValueType,
        std::hash<KeyType>,
        std::equal_to<KeyType>,
        PalallocWrapper<PairType>
    > map(wrapper);

    std::cout << "===== Basic Insert =====\n";

    map[123] = 456;
    map[456] = 789;

    assert(map[123] == 456);
    assert(map[456] == 789);

    std::cout << "PASS\n";

    std::cout << "\n===== Large Insert =====\n";

    for (int i = 0; i < 1000; ++i) {
        map[i] = i * 2;
    }

    for (int i = 0; i < 1000; ++i) {
        assert(map[i] == i * 2);
    }

    std::cout << "PASS\n";

    std::cout << "\n===== Erase =====\n";

    for (int i = 0; i < 500; ++i) {
        map.erase(i);
    }

    for (int i = 0; i < 500; ++i) {
        assert(map.find(i) == map.end());
    }

    for (int i = 500; i < 1000; ++i) {
        assert(map[i] == i * 2);
    }

    std::cout << "PASS\n";

    std::cout << "\n===== Reinsert =====\n";

    for (int i = 0; i < 500; ++i) {
        map[i] = i * 3;
    }

    for (int i = 0; i < 500; ++i) {
        assert(map[i] == i * 3);
    }

    std::cout << "PASS\n";

    std::cout << "\n===== Iterate =====\n";

    long long sum = 0;

    for (const auto& [k, v] : map) {
        sum += v;
    }

    std::cout << "Value Sum = " << sum << '\n';

    std::cout << "PASS\n";

    std::cout << "\n===== Rehash =====\n";

    auto before = map.bucket_count();

    map.rehash(4096);

    auto after = map.bucket_count();

    std::cout << "Buckets: "
              << before
              << " -> "
              << after
              << '\n';

    for (int i = 0; i < 500; ++i) {
        assert(map[i] == i * 3);
    }

    for (int i = 500; i < 1000; ++i) {
        assert(map[i] == i * 2);
    }

    std::cout << "PASS\n";

    std::cout << "\n===== Final Stats =====\n";

    std::cout << "Size         : " << map.size() << '\n';
    std::cout << "Bucket Count : " << map.bucket_count() << '\n';
    std::cout << "Load Factor  : " << map.load_factor() << '\n';

    std::cout << "\nALL TESTS PASSED\n";
}