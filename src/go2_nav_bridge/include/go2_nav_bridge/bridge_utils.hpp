// Copyright 2026 Alessandro Biagio Branca
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GO2_NAV_BRIDGE__BRIDGE_UTILS_HPP_
#define GO2_NAV_BRIDGE__BRIDGE_UTILS_HPP_

#include <chrono>
#include <cstdint>

namespace go2_nav_bridge
{

constexpr double kLinearMax = 1.0;
constexpr double kAngularMax = 1.0;
constexpr uint8_t kVelocityMoveMode = 2;
constexpr double kDefaultWatchdogMs = 200.0;
constexpr double kStopRepublishMs = 500.0;

double validate_positive_or_fallback(double value, double fallback);
double sanitize_finite_or_zero(double value);
float clamp_symmetric(double value, double limit);

std::chrono::milliseconds watchdog_period_from_timeout(double timeout_ms);
bool watchdog_timeout_elapsed(double elapsed_ms, double watchdog_timeout_ms);

}  // namespace go2_nav_bridge

#endif  // GO2_NAV_BRIDGE__BRIDGE_UTILS_HPP_
