#pragma once
#include <memory>
#include <string>

#include "../core/cat_brain.hpp"
#include "../render/cat_renderer.hpp"
#include "input_relay.hpp"
#include "ports.hpp"

namespace gato {

/** The one clock everything uses, in milliseconds */
double monotonicMs();

/**
 * The composition every platform shares: settings, the brain, the drawing,
 * dragging, petting and the menu. A platform feeds it pointer events and
 * calls tick(); it answers how long it can sleep.
 */
class CatApp {
 public:
  CatApp(std::string assetDir, SettingsStore& store, InputSource& input, CatWindow& window);

  /** Opens the window at its saved (or default) place and starts listening */
  void start(double nowMs);
  void stop();

  /** Pointer events over the cat window: window pixels and screen pixels */
  void pointerDown(MouseButton button, double x, double y, double screenX, double screenY, double nowMs);
  void pointerMove(double x, double y, double screenX, double screenY, double nowMs);
  void pointerUp(MouseButton button, double nowMs);

  /** Runs whatever is due; returns milliseconds until it is needed again */
  double tick(double nowMs);

  const Settings& settings() const { return settings_; }
  /** Window size for the current settings */
  std::pair<int, int> size() const;

 private:
  void apply(const Settings& next);
  void onInput(const CatInput& input);
  void wake() { nextFrame_ = 0; }

  std::string assetDir_;
  SettingsStore& store_;
  CatWindow& window_;
  Settings settings_;
  CatBrain brain_;
  CatRenderer renderer_;
  InputRelay relay_;
  Image frame_;
  double now_ = 0;
  double nextFrame_ = 0;
  double nextCursorPoll_ = 0;
  std::optional<Point> lastCursor_;
  struct Drag { double sx, sy; Point origin; bool moved; };
  std::optional<Drag> drag_;
};

}  // namespace gato
