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

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <go2_nav_bridge/bridge_utils.hpp>

// --- validate_positive_or_fallback ---

TEST(BridgeUtils, ValidatePositiveNormal)
{
  EXPECT_DOUBLE_EQ(go2_nav_bridge::validate_positive_or_fallback(2.5, 1.0), 2.5);
}

TEST(BridgeUtils, ValidatePositiveZeroReturnsFallback)
{
  EXPECT_DOUBLE_EQ(go2_nav_bridge::validate_positive_or_fallback(0.0, 1.0), 1.0);
}

TEST(BridgeUtils, ValidatePositiveNegativeReturnsFallback)
{
  EXPECT_DOUBLE_EQ(go2_nav_bridge::validate_positive_or_fallback(-3.0, 1.0), 1.0);
}

TEST(BridgeUtils, ValidatePositiveNaNReturnsFallback)
{
  EXPECT_DOUBLE_EQ(
    go2_nav_bridge::validate_positive_or_fallback(std::numeric_limits<double>::quiet_NaN(), 1.0),
    1.0);
}

TEST(BridgeUtils, ValidatePositiveInfReturnsFallback)
{
  EXPECT_DOUBLE_EQ(
    go2_nav_bridge::validate_positive_or_fallback(std::numeric_limits<double>::infinity(), 1.0),
    1.0);
}

// --- sanitize_finite_or_zero ---

TEST(BridgeUtils, SanitizeFiniteNormal)
{
  EXPECT_DOUBLE_EQ(go2_nav_bridge::sanitize_finite_or_zero(0.5), 0.5);
}

TEST(BridgeUtils, SanitizeFiniteNaN)
{
  EXPECT_DOUBLE_EQ(
    go2_nav_bridge::sanitize_finite_or_zero(std::numeric_limits<double>::quiet_NaN()), 0.0);
}

TEST(BridgeUtils, SanitizeFiniteInf)
{
  EXPECT_DOUBLE_EQ(
    go2_nav_bridge::sanitize_finite_or_zero(std::numeric_limits<double>::infinity()), 0.0);
}

TEST(BridgeUtils, SanitizeFiniteNegInf)
{
  EXPECT_DOUBLE_EQ(
    go2_nav_bridge::sanitize_finite_or_zero(-std::numeric_limits<double>::infinity()), 0.0);
}

// --- clamp_symmetric ---

TEST(BridgeUtils, ClampSymmetricWithinRange)
{
  EXPECT_FLOAT_EQ(go2_nav_bridge::clamp_symmetric(0.5, 1.0), 0.5F);
}

TEST(BridgeUtils, ClampSymmetricAboveMax)
{
  EXPECT_FLOAT_EQ(go2_nav_bridge::clamp_symmetric(2.0, 1.0), 1.0F);
}

TEST(BridgeUtils, ClampSymmetricBelowMin)
{
  EXPECT_FLOAT_EQ(go2_nav_bridge::clamp_symmetric(-2.0, 1.0), -1.0F);
}

TEST(BridgeUtils, ClampSymmetricNegativeLimit)
{
  // Negative limit should be treated as abs(limit)
  EXPECT_FLOAT_EQ(go2_nav_bridge::clamp_symmetric(2.0, -1.0), 1.0F);
  EXPECT_FLOAT_EQ(go2_nav_bridge::clamp_symmetric(-2.0, -1.0), -1.0F);
}

TEST(BridgeUtils, ClampSymmetricZeroLimit)
{
  EXPECT_FLOAT_EQ(go2_nav_bridge::clamp_symmetric(5.0, 0.0), 0.0F);
}

// --- watchdog_period_from_timeout ---

TEST(BridgeUtils, WatchdogPeriodNormal)
{
  auto period = go2_nav_bridge::watchdog_period_from_timeout(200.0);
  EXPECT_EQ(period.count(), 100);
}

TEST(BridgeUtils, WatchdogPeriodClampedToMin)
{
  auto period = go2_nav_bridge::watchdog_period_from_timeout(0.5);
  EXPECT_EQ(period.count(), 1);
}

TEST(BridgeUtils, WatchdogPeriodClampedToMax)
{
  auto period = go2_nav_bridge::watchdog_period_from_timeout(1e18);
  EXPECT_EQ(period.count(), 10000);
}

TEST(BridgeUtils, WatchdogPeriodNegativeClampedToMin)
{
  auto period = go2_nav_bridge::watchdog_period_from_timeout(-100.0);
  EXPECT_EQ(period.count(), 1);
}

// --- watchdog_timeout_elapsed ---

TEST(BridgeUtils, WatchdogTimeoutNotElapsed)
{
  EXPECT_FALSE(go2_nav_bridge::watchdog_timeout_elapsed(100.0, 200.0));
}

TEST(BridgeUtils, WatchdogTimeoutElapsed)
{
  EXPECT_TRUE(go2_nav_bridge::watchdog_timeout_elapsed(300.0, 200.0));
}

TEST(BridgeUtils, WatchdogTimeoutExactBoundary)
{
  // At exact boundary, should NOT be elapsed (> not >=)
  EXPECT_FALSE(go2_nav_bridge::watchdog_timeout_elapsed(200.0, 200.0));
}
