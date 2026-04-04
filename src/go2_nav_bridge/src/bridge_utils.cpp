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

#include <go2_nav_bridge/bridge_utils.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace go2_nav_bridge
{

double validate_positive_or_fallback(double value, double fallback)
{
  if (!std::isfinite(value) || value <= 0.0) {
    return fallback;
  }

  return value;
}

double sanitize_finite_or_zero(double value)
{
  if (!std::isfinite(value)) {
    return 0.0;
  }

  return value;
}

float clamp_symmetric(double value, double limit)
{
  const double abs_limit = std::abs(limit);
  return static_cast<float>(std::clamp(value, -abs_limit, abs_limit));
}

std::chrono::milliseconds watchdog_period_from_timeout(double timeout_ms)
{
  constexpr double kMaxPeriodMs = 10000.0;
  const double half = std::clamp(timeout_ms / 2.0, 1.0, kMaxPeriodMs);
  return std::chrono::milliseconds(static_cast<int64_t>(half));
}

bool watchdog_timeout_elapsed(double elapsed_ms, double watchdog_timeout_ms)
{
  return elapsed_ms > watchdog_timeout_ms;
}

}  // namespace go2_nav_bridge
