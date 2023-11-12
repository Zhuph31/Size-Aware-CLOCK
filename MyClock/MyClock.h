/*
 * LruClockCache.h
 *
 *  Created on: Oct 4, 2021
 *      Author: tugrul
 */

#ifndef MYCLOCKCACHE_H_
#define MYCLOCKCACHE_H_

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "../LruClockCache/LruClockCache.h"

class ExponentialSmoothedThreshold {
public:
  ExponentialSmoothedThreshold(double alpha)
      : alpha_(alpha), threshold_(0.0), initialized_(false) {}

  void update_threshold(double observation) {
    if (!initialized_) {
      threshold_ = observation;
      initialized_ = true;
    } else {
      threshold_ = alpha_ * observation + (1.0 - alpha_) * threshold_;
    }
  }

  // 获取当前阈值
  double get_threshold() const { return threshold_; }

private:
  double alpha_;
  double threshold_;
  bool initialized_;
};

template <typename LruKey, typename LruValue,
          typename ClockHandInteger = size_t>
class MyClockCache : public LruClockCache<LruKey, LruValue, ClockHandInteger> {
public:
  MyClockCache(double alpha, ClockHandInteger numElements,
               ClockHandInteger memLimit,
               const std::function<LruValue(LruKey)> &readMiss,
               const std::function<void(LruKey, LruValue)> &writeMiss)
      : threshold_(alpha), LruClockCache<LruKey, LruValue, ClockHandInteger>(
                               numElements, memLimit, readMiss, writeMiss) {

    this->shouldAdopt = [this](size_t size) {
      return threshold_.get_threshold() >= size;
    };
  }

  // inline const LruValue get(const LruKey &key) noexcept {
  inline const LruValue get(const LruKey &key) noexcept override {
    LruValue value = this->accessClock2Hand(key, nullptr);
    this->controlMemUsage();
    // always update threshold
    threshold_.update_threshold(key.size());
    return value;
  }

private:
  ExponentialSmoothedThreshold threshold_;
};

#endif /* MYCLOCKCACHE_H_ */
