#include <fstream>
#include <gflags/gflags.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "LruClockCache/LruClockCache.h"
#include "MyClock/MyClock.h"
#include "basic_lru/lrucache.hpp"
#include "utils.h"

DEFINE_int64(storage_size, 10000, "");
DEFINE_int64(iterations, 10000, "");
DEFINE_int64(cache_size, 1000, "");
DEFINE_double(alpha, 0.2, "");
DEFINE_string(file, "hm_0.csv", "");

using key_type = std::string;
using value_type = std::string;

enum class VisitType {
  READ = 0,
  WRITE = 1,
};

struct Row {
  long timestamp; // 100ns
  VisitType type;
  std::string offset;
  long length;

  Row() = delete;
  Row(long ts, VisitType t, const std::string &o, long l)
      : timestamp(ts), type(t), offset(o), length(l) {}
  void debug() const {
    printf("%ld,%d,%s,%ld\n", timestamp, type, offset.c_str(), length);
  }
};

std::unordered_map<std::string /* offset */, Row> storage;

void init_storage() {
  std::ifstream file(FLAGS_file);
  if (!file.is_open()) {
    exit(-2);
  }
  std::string line;

  std::vector<Row> rows;
  rows.reserve(4000000);

  long begin_ts = -1;

  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string value;
    std::vector<std::string> values;
    while (std::getline(iss, value, ',')) {
      values.push_back(value);
    }

    const std::string &ts = values[0], type = values[3], offset = values[4],
                      length = values[5];
    long relative_ts = 0;

    if (begin_ts != -1) {
      relative_ts = std::stol(ts) - begin_ts;
    } else {
      begin_ts = std::stol(ts);
    }
    rows.emplace_back(Row(relative_ts,
                          type == "Read" ? VisitType::READ : VisitType::WRITE,
                          offset, std::stol(length)));
  }

  int count = 0;
  for (const Row &row : rows) {
    row.debug();
    ++count;
    if (count > 10) {
      break;
    }
  }

  printf("generated storage, size:%ld\n", rows.size());
}

std::pair<size_t, long long> test_basic_lru() {
  cache::lru_cache<std::string, std::string> cache(FLAGS_cache_size, storage);
  TimeCost tc;

  for (const key_type &each : test_keys) {
    cache.get(each);
  }

  return {cache.get_miss(), tc.get_elapsed()};
}

std::pair<size_t, long long> test_clock_lru() {
  int miss = 0;
  auto read_miss = [&](key_type key) {
    ++miss;
    return storage.at(key);
  };
  auto write_miss = [&](key_type key, value_type value) {
    storage[key] = value;
    exit(-1);
  };
  LruClockCache<key_type, value_type> cache(FLAGS_cache_size, read_miss,
                                            write_miss);

  TimeCost tc;
  for (const key_type &each : test_keys) {
    cache.get(each);
  }
  return {miss, tc.get_elapsed()};
}

std::pair<size_t, long long> test_my_clock() {
  int miss = 0;
  auto read_miss = [&](key_type key) {
    ++miss;
    return storage.at(key);
  };
  auto write_miss = [&](key_type key, value_type value) {
    storage[key] = value;
    exit(-1);
  };
  MyClockCache<key_type, value_type> cache(FLAGS_alpha, FLAGS_cache_size,
                                           read_miss, write_miss);

  TimeCost tc;
  for (const key_type &each : test_keys) {
    cache.get(each);
  }
  return {miss, tc.get_elapsed()};
}

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  init_storage();

  size_t miss;
  long long cost;

  std::tie(miss, cost) = test_basic_lru();
  printf("basic lru, miss:%lu, cost:%lld\n", miss, cost);
  std::tie(miss, cost) = test_clock_lru();
  printf("clock lru, miss:%lu, cost:%lld\n", miss, cost);
  std::tie(miss, cost) = test_my_clock();
  printf("my lru, miss:%lu, cost:%lld\n", miss, cost);

  return 0;
}