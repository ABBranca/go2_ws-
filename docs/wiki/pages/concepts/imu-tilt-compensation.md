---
type: concept
tags: [slam, imu, navigation, sensor]
aliases: [IMU tilt compensation, base_stabilized, gravity leveling, scan slab leveling, complementary filter, gait tilt]
updated: 2026-06-03
---

# IMU Tilt Compensation — Gravity-Levelled Scan Slab

Compensation of **gait-induced body tilt** in the [[slam-toolbox-2d|2D SLAM]] pipeline by
projecting the LiDAR-to-laserscan flattening into a **gravity-levelled frame**
(`base_stabilized`) whose roll/pitch are derived from the [[imu]], not from the leg
odometry. Implemented in `odom_tf_broadcaster` (2026-06-03). This page is the full
theoretical + implementation reference; it resolves [[slam-toolbox-2d]] open-risk #2 and
supersedes the leg-odom levelling that regressed in [[2d-map-tuning]].

---

## 1. Problem: the scan slab tilts with the gait

The 2D pipeline flattens the 3D [[hesai-xt16]] cloud into a planar `/scan` with
`pointcloud_to_laserscan`. The flattener works by (i) transforming every point into a
`target_frame`, (ii) discarding points outside a vertical **height band**
`z ∈ [min_height, max_height]` (here `[0.0, 0.45] m`), (iii) collapsing the survivors onto
the XY plane and keeping, per azimuth bin, the **minimum range**.

The XT-16 is mounted **horizontal** ([[extrinsics]]: `R = I₃`, `T = [0.171, 0, 0.0908] m`),
so a level body produces a horizontal measurement ring. The band then samples a clean
horizontal slab of walls/furniture.

During a quadruped gait the body **pitches and rolls** continuously (estimated O(1–5°)
peak). If the flattening `target_frame` is the body frame `base_link`, the band is defined
**relative to the tilting body**, not to gravity. Two failure mechanisms follow:

1. **Vertical drift of the sampled ring.** A point at horizontal distance `x` on the ring
   moves in body-`z` by `Δz ≈ x·sin(θ)` under a pitch `θ`. At `θ = 5°` and `x = 3 m`,
   `Δz ≈ 0.26 m` — comparable to the whole band width. Floor and ceiling returns drift
   *into* the band; wall returns drift *out*.
2. **Min-range corruption.** Because the flattener keeps the **minimum** range per azimuth,
   a single spurious floor strike that enters the band overrides the true (farther) wall
   range → a phantom short reading → **ragged walls**, and a noisy, gait-correlated scan
   that the front-end scan-matcher must absorb as apparent motion → **drift** in the final
   `map→odom` (the canonical quality metric, [[2d-map-tuning]]).

> The fix is **not** a vertical state estimate (the LiDAR cannot observe Z —
> [[z-observability]]). It is a *projection* fix: rotate the band's reference frame back to
> gravity so the slab stays horizontal in the world regardless of body tilt.

---

## 2. Why the leg odometry is the wrong tilt source (the prior regression)

`odom_tf_broadcaster` already publishes a `base_link → base_stabilized` transform that
removes roll/pitch and keeps yaw. The **first** attempt ([[2d-map-tuning]], run `level1`)
took that roll/pitch from the **leg-odometry attitude** (`/utlidar/robot_odom`
orientation). It **regressed** the map: final `map→odom` yaw went **+5.8° → +12.9°**.

Cause: the Go2 leg/contact-kinematic attitude estimate carries a **static roll bias of
≈ −2.6°** (measured while the robot stood physically level) plus per-step noise.
De-tilting by a biased, noisy attitude injects a gait-correlated rotational jitter into the
scan frame — *worse* than not levelling at all. The leg estimator's planar state (X, Y,
yaw) is an excellent odometry prior, but its **roll/pitch is not** trustworthy.

The accelerometer, by contrast, measures the gravity direction **absolutely**: roll/pitch
are *observable* and bias-bounded. Hence the wiki verdict made concrete here: *"a levelled
frame needs the [[imu]], not `/utlidar/robot_odom`."*

---

## 3. Sensor model: what the IMU measures

A strap-down IMU accelerometer does not measure acceleration — it measures **specific
force** `f`, the non-gravitational force per unit mass, expressed in the body frame `b`:

```
f_b = R_bw · ( a_w − g_w )            g_w = [0, 0, −g]ᵀ ,  g ≈ 9.81 m/s²
```

where `a_w` is the body's kinematic acceleration in the world frame `w`, `g_w` is the
gravity acceleration vector (pointing down), and `R_bw` rotates world→body. At rest
(`a_w = 0`):

```
f_b = −R_bw · g_w = R_bw · [0, 0, +g]ᵀ
```

i.e. a level, stationary IMU reads `f_b = [0, 0, +g]ᵀ` — the **reaction to gravity**,
pointing "up" in the body. This standing gravity vector is a direct, drift-free encoding
of the body's tilt relative to vertical. The gyroscope measures body angular rate `ω_b`.

> **Frame note.** [[extrinsics]] establishes that `base_link` *is* the IMU body reference
> with `R = I₃`. The derivation therefore treats IMU roll/pitch as `base_link` roll/pitch
> directly. If a future calibration finds a non-identity IMU↔base_link rotation, `f_b` and
> `ω_b` must be pre-rotated into `base_link` before the equations below.

---

## 4. Tilt from the accelerometer (gravity vector)

With ZYX Euler angles (yaw `ψ`, pitch `θ`, roll `φ`) and yaw irrelevant to gravity, the
stationary specific-force components give roll and pitch in closed form:

```
φ_acc = atan2( f_y ,  f_z )
θ_acc = atan2( −f_x ,  sqrt( f_y² + f_z² ) )
```

(`f = [f_x, f_y, f_z]ᵀ = linear_acceleration` of the `sensor_msgs/Imu`.) These are exact
for a stationary sensor and singularity-free via `atan2`. The pitch uses the full
magnitude `sqrt(f_y²+f_z²)` in the denominator so it degrades gracefully near `±90°`.

### 4.1 Why the accelerometer alone fails during gait

Equations §4 assume `a_w = 0`. During locomotion the legs inject body acceleration; the
accelerometer then reads `f_b = R_bw(a_w − g_w)`, so the inferred tilt is corrupted. The
first-order error magnitude for a horizontal disturbance `a_h` is:

```
Δφ ≈ a_h / g   [rad]
```

A modest `a_h = 1 m/s²` yields `Δφ ≈ 0.10 rad ≈ 5.8°` — **larger than the −2.6° static
bias we are trying to remove**, and time-varying at the step rate. An accel-only level
would therefore be *noisier than the disease*. The redeeming property: gait acceleration is
broadly **zero-mean over a gait cycle and high-frequency** (step rate ~1–3 Hz), while the
gravity signal we want is **quasi-DC**. The two are separable in frequency.

### 4.2 Why the gyroscope alone fails

Integrating `ω_b` gives a clean, high-bandwidth tilt over short horizons:
`φ ← φ + ω_x·dt`, `θ ← θ + ω_y·dt` (small-angle Euler-rate approximation; the exact map is
`[φ̇,θ̇,ψ̇]ᵀ = T(φ,θ)·ω`, with `T → I` near level). But the gyro **bias random walk**
(`b_gyr ≈ 2.796e-5 rad/s·√Hz`, [[imu]]) integrates into an **unbounded drift** at DC. Gyro
is trustworthy at high frequency, untrustworthy at DC — the exact complement of the
accelerometer.

---

## 5. The complementary filter

Because the two error spectra are complementary (accel good at low frequency, gyro good at
high frequency), a **first-order complementary filter** fuses them with a single crossover.
In the Laplace domain the roll estimate is:

```
              1                      τ
 φ̂(s) =  ───────── · φ_acc(s)  +  ───────── · ω_x(s)
           τs + 1                   τs + 1
          └─ low-pass ─┘          └─ high-pass·∫ ─┘
```

The two weighting functions sum identically to 1 — `1/(τs+1) + τs/(τs+1) ≡ 1` — hence
"complementary": no frequency is double-counted or dropped. The **crossover** is at
`ω_c = 1/τ`, i.e. `f_c = 1/(2πτ)`. Below `f_c` the estimate follows the drift-free
accelerometer gravity; above `f_c` it follows the integrated gyro and the gait
acceleration is rejected.

### 5.1 Discrete implementation

Sampling at the IMU rate (~253 Hz, [[imu]] → `dt ≈ 0.004 s`), the standard discretization
of the filter above is the single-gain recursion actually coded:

```
α        = τ / (τ + dt)                       # ∈ (0,1)
φ̂_k     = α·( φ̂_{k-1} + ω_x·dt ) + (1−α)·φ_acc,k
θ̂_k     = α·( θ̂_{k-1} + ω_y·dt ) + (1−α)·θ_acc,k
```

Equivalently `τ = α·dt /(1−α)`. The default `cf_tau = 0.25 s` gives, at `dt = 0.004 s`,
`α ≈ 0.984` and crossover `f_c ≈ 0.64 Hz` — comfortably below the gait step rate, so the
periodic gait acceleration sits in the gyro-trusted band and is suppressed in the gravity
estimate. Tuning trade-off: **larger `τ`** → smoother, more gait-rejecting, but **laggier**
on genuine tilt; **smaller `τ`** → faster but admits more accel noise. Sweep `0.1–0.5 s`.

### 5.2 Initialization and robustness

- **Seed** the state from the gravity estimate (`φ̂ ← φ_acc`, `θ̂ ← θ_acc`) on the first
  message — avoids a long transient from a zero start.
- **`dt` guards:** skip the integration and resync to `φ_acc/θ_acc` if `dt ≤ 0` (reordered
  stamps) or `dt > 0.1 s` (a gap/dropout), preventing a single bad `dt` from corrupting the
  integrator.

---

## 6. Alternative: a firmware-provided orientation quaternion

Some Unitree firmwares populate `sensor_msgs/Imu.orientation` with an already-fused,
gravity-referenced quaternion. The node detects this on the first message:
`orientation_covariance[0] == -1` signals "no orientation" (REP-145) → use the
complementary filter (§5); otherwise extract roll/pitch directly from the quaternion and
**discard yaw**. The `imu_mode` parameter forces either path; `auto` selects per the
covariance flag. **Verification on the robot is required** —
`ros2 topic echo /utlidar/imu --once`, inspect `orientation_covariance[0]`.

---

## 7. Why only roll/pitch — never yaw

The accelerometer constrains the two tilt DOF (gravity is a 2-DOF direction). It says
**nothing about heading**: rotating about the gravity axis leaves `f_b` unchanged. Without
a magnetometer the IMU yaw is unobservable and integrates gyro drift. Heading must
therefore stay owned by the leg-odometry → `slam_toolbox` chain (whose X/Y/**yaw** is a
good prior). The levelled frame zeroes yaw **by construction**:

```
q_tilt = R_z(0) · R_y(θ_corr) · R_x(φ_corr)        # yaw = 0 deliberately
```

---

## 8. Static bias calibration

Even a perfect gravity estimate inherits any **mechanical misalignment** between the IMU
axes and the `base_link` reference (machining/mount tolerance), plus any residual firmware
offset. This is the source of the ≈ −2.6° class of constant tilt. It is removed by a
**one-time static calibration**:

1. Stand the robot on a **provably level** surface (spirit level on the back plate).
2. Record a few seconds of `/utlidar/imu`; average the inferred `(φ, θ)`.
3. Set `roll_offset`, `pitch_offset` [rad] to those means.

The node subtracts them before building the frame:
`φ_corr = φ − roll_offset`, `θ_corr = θ − pitch_offset`. Without this, a constant tilt
bakes into **every** scan — exactly the failure mode of the leg-odom attempt, now corrected
against gravity rather than against the legs.

---

## 9. Frame construction and TF math

`base_stabilized` is `base_link` with the corrected tilt undone and yaw preserved — i.e. a
frame sharing the body origin and heading but **levelled to gravity**. The published
transform is the rotation of the `base_stabilized` axes expressed in `base_link`:

```
T(base_link → base_stabilized).rotation = q_tilt⁻¹       (translation = 0)
```

Derivation: the body is the level frame rotated by `q_tilt` about the level axes, so the
level frame seen from the body is `q_tilt⁻¹`. The translation is zero — same origin, pure
de-rotation. The transform is **stamped with the IMU `header.stamp`** so it shares the
sensor's time domain (critical — see §12).

For comparison, the `tilt_source = odom` path derives the same `q_tilt⁻¹` from the
leg-odom quaternion by splitting off yaw (`q_tilt = q_yaw⁻¹·q`), retained for A/B testing
and back-compatibility.

---

## 10. Integration in the pipeline + frame consistency

```
/utlidar/imu ─> [odom_tf_broadcaster] ─ TF base_link→base_stabilized (~253 Hz)
/lidar_points ─> [pointcloud_to_laserscan, target_frame=base_stabilized] ─> /scan (frame_id=base_stabilized)
                                                                              └─> [slam_toolbox]
```

- `pointcloud_to_laserscan.yaml`: `target_frame: base_stabilized` (was `base_link`). The
  flattener now transforms the cloud (via static `hesai_lidar→base_link` ∘ dynamic
  `base_link→base_stabilized`) into the **levelled** frame, so the height band gates on true
  gravity-vertical height and the projected ranges are level-plane ranges — gait-invariant.
- **slam_toolbox keeps `base_frame: base_link`.** `/scan` now carries
  `frame_id: base_stabilized`; slam_toolbox resolves the laser pose through
  `base_stabilized → base_link → odom`, which exists. Only the **scan projection plane** is
  levelled; the tracked body pose and the pose graph are unchanged. **Do not** change
  `base_frame` to `base_stabilized` — that would conflate the levelled sensor frame with the
  body pose and break the odometry chain.

---

## 11. Parameter reference (`odom_tf_broadcaster`)

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `tilt_source` | `odom` | `imu` = derive roll/pitch from `/utlidar/imu`; `odom` = legacy leg-odom split. Launches set `imu`. |
| `publish_stabilized` | `true` | Emit `base_link → stabilized_frame`. |
| `stabilized_frame` | `base_stabilized` | Child frame name of the levelled transform. |
| `imu_topic` | `/utlidar/imu` | IMU input (used only when `tilt_source=imu`). |
| `imu_mode` | `auto` | `auto` picks by `orientation_covariance[0]`; `quaternion` trusts firmware orientation; `complementary` fuses raw accel+gyro. |
| `cf_tau` | `0.25` | Complementary-filter time constant τ [s] → `α=τ/(τ+dt)`, crossover `1/(2πτ)`. |
| `roll_offset` | `0.0` | Static roll bias [rad] subtracted (level calibration, §8). |
| `pitch_offset` | `0.0` | Static pitch bias [rad] subtracted. |
| `imu_qos_reliable` | `false` | `false` = SensorDataQoS (best-effort, matches a sensor publisher); `true` = reliable. |

---

## 12. Validation protocol

**Use a WALKING bag.** The 2026-06-03 sweep ([[2d-map-tuning]]) ran a near-static slow
trajectory where gait tilt is negligible — leveling can only be judged where the body
actually pitches/rolls.

**Clock-domain prerequisite ([[clock-domain-mismatch]]).** `/utlidar/imu` is published by
the **MCU firmware**, sharing the clock domain of `/utlidar/robot_odom` (2026 UTC), **not**
the unsynced Orin clock that stamps the Hesai cloud (~1970). On a raw replayed bag the
`base_stabilized` transform (IMU-stamped, 2026) and `/scan` (cloud-stamped, 1970) fall in
disjoint time domains → `pointcloud_to_laserscan` cannot find the transform → **every scan
drops**. Replay a **re-stamped** bag (`bags/restamp_bag.py`); first confirm the script
re-stamps `/utlidar/imu` into the cloud domain (add it to the topic set if missing), or
record a single-clock bag after NTP/PTP sync.

**Procedure:**
1. `colcon build --packages-select go2_nav_bridge`, source the overlay.
2. `ros2 launch go2_nav_bridge mapping_2d_replay.launch.py` against the re-stamped walking
   bag, with `ros2 bag play <bag> --clock` and `use_sim_time:=true` on all nodes.
3. **A/B/C** on the same bag: `tilt_source:=imu` vs `tilt_source:=odom` vs un-levelled
   baseline (`target_frame:=base_link`).
4. **Metrics** (harness `bags/run_2d_replay_test.sh`, `bags/map_stats.py`):
   - final `map→odom` **yaw** — smaller = less gait-induced drift absorbed by the matcher;
     baseline reference `res03 = −2.9°`. Target: IMU path ≤ baseline.
   - known-coverage % and wall density (occupied/known).
5. **Live de-tilt sanity:** `ros2 run tf2_tools view_frames` must show `base_stabilized`;
   echoing its roll/pitch (e.g. `tf2_echo base_link base_stabilized`) while the body tilts
   should show it counter-rotating to stay near gravity-level.

---

## 13. Failure modes and diagnostics

| Symptom | Likely cause | Check / fix |
|---------|--------------|-------------|
| All scans dropped, empty `/map` | clock-domain mismatch (§12) | re-stamp bag incl. `/utlidar/imu`; `bags/dump_stamps.py` |
| Tilt **amplified** not cancelled | accel sign / gyro axis convention wrong | echo `base_stabilized` vs raw IMU; verify level read `f_b ≈ [0,0,+9.81]`; flip sign if needed |
| Constant residual tilt in map | `roll_offset/pitch_offset` not calibrated | run §8 calibration |
| `base_stabilized` lags real tilt | `cf_tau` too large | reduce τ toward 0.1 s |
| Jittery, noisy levelling while walking | `cf_tau` too small (admits accel) | increase τ toward 0.5 s |
| No `base_stabilized` at all | `imu_mode=quaternion` but firmware sends none, or `tilt_source≠imu` | use `auto`; confirm `orientation_covariance[0]` |

---

## 14. Limitations

- **Roll/pitch only.** Yaw and Z are out of scope by design (§7, [[z-observability]]).
- **Projection fix, not perception.** Obstacles outside the (now level) slab remain
  unmapped — inherent to a single 2D slice from a 16-beam sensor.
- **Single IMU, unknown model.** No redundancy; characterised only by Allan variance
  ([[imu]]). A persistent accel bias beyond the static offset would leak in.
- **Empirical validation pending** a walking bag; the design is sound but the numerical
  gain over `res03 = −2.9°` is unmeasured (see [[project_imu_tilt_leveling]] tracking).

---

## 15. References

- Valenti, R.G.; Dryanovski, I.; Xiao, J. *"Keeping a Good Attitude: A Quaternion-Based
  Orientation Filter for IMUs and MARGs."* **Sensors** 15(8), 2015. — the quaternion
  complementary filter; the scalar roll/pitch form here is its tilt projection.
- Higgins, W.T. *"A Comparison of Complementary and Kalman Filtering."* **IEEE Trans.
  Aerospace and Electronic Systems**, 1975. — equivalence of the complementary filter to a
  steady-state Kalman filter.
- Titterton, D.; Weston, J. *Strapdown Inertial Navigation Technology*, 2nd ed., IET, 2004.
  — specific-force sensor model (§3) and the Euler-rate kinematics (§4.2).
- REP-103 (units/axes), REP-105 (coordinate frames `map`/`odom`/`base_link`), REP-145
  (IMU message conventions, the `covariance[0]=-1` "unknown" flag).

---

## Related
- [[slam-toolbox-2d]] — the 2D stack; this resolves its open-risk #2
- [[2d-map-tuning]] — the lab sweep where leg-odom levelling regressed (the motivation)
- [[imu]] — the sensor: topic, rate, noise model
- [[extrinsics]] — IMU/LiDAR ↔ base_link transforms (R=I₃ assumption)
- [[z-observability]] — why the stack is 2D and Z is never estimated
- [[tf-tree]] — the frame chain `base_stabilized` slots into
- [[clock-domain-mismatch]] — the replay prerequisite for IMU-stamped frames
- [[motion-undistortion]] — the *other* IMU-aided correction (intra-scan de-skew), distinct from inter-scan tilt levelling

<!-- created 2026-06-03: IMU-levelled base_stabilized implemented in odom_tf_broadcaster (tilt_source=imu, complementary filter); resolves slam-toolbox-2d open-risk #2; supersedes leg-odom levelling regression from 2d-map-tuning -->
