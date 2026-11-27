#pragma once
#include <functional>

#include "../core/types.hpp"
#include "ports.hpp"

namespace gato {

/**
 * Turns raw OS input into what the cat understands. Keeps a pointer
 * estimate (Wayland only gives relative motion) and throttles gaze updates.
 * Single threaded: the platform loop calls tick() regularly.
 */
class InputRelay {
 public:
  using Send = std::function<void(const CatInput&)>;
  using OnDevices = std::function<void(const Devices&)>;

  InputRelay(InputSource& source, std::function<Point()> head, std::function<Rect()> desktop, Send send, OnDevices onDevices);

  void start(double nowMs);
  void stop();
  /** Exact pointer position, when the platform can tell it */
  void syncPointer(Point p, double nowMs);
  /** Sends a pending gaze once its throttle interval is over */
  void tick(double nowMs);
  /** Milliseconds until tick() has something to do, or -1 */
  double nextDue(double nowMs) const;
  const Devices& devices() const { return devices_; }

 private:
  void onRaw(const RawInput& raw);
  void scheduleGaze(double nowMs);

  InputSource& source_;
  std::function<Point()> head_;
  std::function<Rect()> desktop_;
  Send send_;
  OnDevices onDevices_;
  Point pointer_{0, 0};
  double gazeDue_ = -1;
  double now_ = 0;
  Devices devices_;
};

}  // namespace gato
