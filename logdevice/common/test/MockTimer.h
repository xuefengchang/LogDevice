/**
 * Copyright (c) 2017-present, Facebook, Inc. and its affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#pragma once

#include "logdevice/common/Timer.h"

namespace facebook { namespace logdevice {

/**
 * @file No-op implementation of Timer.
 */

class MockTimer : public Timer {
 public:
  MockTimer() = default;

  explicit MockTimer(std::function<void()> callback) {
    callback_ = std::move(callback);
  }

  void assign(std::function<void()> callback) override {
    callback_ = std::move(callback);
  }
  void activate(std::chrono::microseconds delay,
                TimeoutMap* /*timeout_map*/ = nullptr) override {
    delay_ = delay;
    active_ = true;
  }

  void cancel() override {
    active_ = false;
  }

  bool isActive() const override {
    return active_;
  }

  bool isAssigned() const override {
    return false;
  }

  void setCallback(std::function<void()> callback) override {
    callback_ = std::move(callback);
  }

  void trigger() {
    ld_check(isActive());
    ld_check(callback_);
    cancel();
    callback_();
  }

  std::chrono::microseconds getCurrentDelay() const {
    return delay_;
  }

 private:
  std::function<void()> callback_;
  bool active_ = false;
  std::chrono::microseconds delay_{0};
};

}} // namespace facebook::logdevice