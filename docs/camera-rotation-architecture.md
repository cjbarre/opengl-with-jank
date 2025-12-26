# Camera and Character Rotation Architecture

## Overview

This document compares the character rotation and camera systems between Jedi Academy (a professional game engine) and our Jank/OpenGL implementation, documenting the conceptual models and providing recommendations for improvement.

---

## Conceptual Model: Jedi Academy Character Rotation System

### Architecture Overview

The Jedi Academy system uses a **client-server predictive architecture** with authoritative angle validation:

```
┌─────────────────┐
│   Client Side   │
│                 │
│ 1. Raw Input    │  Mouse/Keyboard → Raw deltas (dx, dy)
│ 2. View Angles  │  Accumulate into cl.viewangles[] (floats)
│ 3. Command Gen  │  Convert to shorts: cmd.angles[] = ANGLE2SHORT(viewangles)
│ 4. Network Send │  Send usercmd_t to server
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Server Side    │
│                 │
│ 5. Reconstruction│ viewangles = SHORT2ANGLE(cmd.angles + delta_angles)
│ 6. Validation   │  Clamp pitch/yaw based on context (vehicle, emplaced, etc.)
│ 7. Delta Update │  Adjust delta_angles if clamped
│ 8. Player State │  Store in ps->viewangles[], send back to client
└─────────────────┘
```

### Key Components

#### 1. Input Collection (`code/client/cl_input.cpp`)

**Mouse Input** - `CL_MouseEvent()` (line 448):
- Captures raw mouse delta movements (dx, dy)
- Stores in `cl.mouseDx[]` and `cl.mouseDy[]` arrays

**Keyboard Input** - Button states tracked via:
- `in_left`, `in_right` - Turn left/right keys
- `in_lookup`, `in_lookdown` - Look up/down keys

#### 2. Angle Adjustment (`code/client/cl_input.cpp`)

**`CL_AdjustAngles()`** (line 359) - Handles keyboard turning:
```c
cl.viewangles[YAW] -= speed*cl_yawspeed->value*CL_KeyState(&in_right);
cl.viewangles[YAW] += speed*cl_yawspeed->value*CL_KeyState(&in_left);
cl.viewangles[PITCH] -= speed*cl_pitchspeed->value*CL_KeyState(&in_lookup);
cl.viewangles[PITCH] += speed*cl_pitchspeed->value*CL_KeyState(&in_lookdown);
```

**`CL_MouseMove()`** (line 550) - Handles mouse turning:
```c
// Yaw (horizontal rotation)
cl.viewangles[YAW] -= m_yaw->value * mx;

// Pitch (vertical rotation)
cl.viewangles[PITCH] += pitch * my * cl_pitchSensitivity;
```
- Uses mouse sensitivity (`cl_sensitivity`) and acceleration (`cl_mouseAccel`)
- Scales by FOV via `cl.cgameSensitivity`

**`CL_CreateCmd()`** (line 764) - Combines all inputs:
1. Calls `CL_AdjustAngles()` for keyboard
2. Calls `CL_MouseMove()` for mouse
3. Calls `CL_JoystickMove()` for joystick
4. Clamps pitch to ±90 degrees (lines 787-791)
5. Converts final angles to shorts via `ANGLE2SHORT()` (line 753)

#### 3. Command Transmission (`code/client/cl_input.cpp`)

**`CL_FinishMove()`** (line 732):
```c
for (i=0; i<3; i++) {
    cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);
}
```
- Packages angles into `usercmd_t` structure
- Sent to server via `CL_WritePacket()` (line 901)

#### 4. Server-Side Processing (`code/game/bg_pangles.cpp`)

**`PM_UpdateViewAngles()`** (line 1349) - Core angle update:
```c
// Reconstruct view angles from command + delta
temp = cmd->angles[i] + ps->delta_angles[i];

// Clamp pitch to limits (default -75 to +75 degrees)
if (i == PITCH) {
    if (temp > pitchClampMax) {
        ps->delta_angles[i] = (pitchClampMax - cmd->angles[i]) & 0xffff;
        temp = pitchClampMax;
    }
    // ... similar for min
}

// Convert back to viewangles
ps->viewangles[i] = SHORT2ANGLE(temp);
```

**Delta Angles System**:
- `ps->delta_angles[]` stores angular offsets
- Formula: `viewangles = SHORT2ANGLE(cmd->angles + delta_angles)`
- Allows server to force view changes (e.g., vehicle control, teleports)
- Used for pitch/yaw clamping based on context (emplaced guns, vehicles, NPCs)

#### 5. Player Movement (`code/game/bg_pmove.cpp`)

Called via `PmoveSingle()` → `PM_UpdateViewAngles()` (line 14959):
- Updates player's actual view direction
- Applies context-specific limits (vehicles, emplaced weapons, etc.)
- Converts angles to forward/right/up vectors for movement (line 14961)

### Key CVars

- `cl_yawspeed` - Keyboard turn speed (yaw)
- `cl_pitchspeed` - Keyboard look speed (pitch)
- `cl_sensitivity` - Base mouse sensitivity
- `cl_mouseAccel` - Mouse acceleration
- `m_yaw` - Mouse yaw sensitivity
- `m_pitch` - Mouse pitch sensitivity (can invert)
- `cl_anglespeedkey` - Speed multiplier when speed key held

### Third-Person Camera System (`code/cgame/cg_view.cpp`)

**Important**: The camera system is **separate** from character rotation.

**Main Function**: `CG_OffsetThirdPersonView()` (line 705)
- Handles third-person camera positioning and orientation each frame

**Key Camera Calculation Functions**:

1. **`CG_CalcIdealThirdPersonViewTarget()`** (line 327)
   - Calculates where the camera should look (focus point on player)
   - Sets target to player's eye position (`viewheight`)
   - Adds vertical offset via `cg_thirdPersonVertOffset`
   - Handles special cases (crouching, being held by creatures)

2. **`CG_CalcIdealThirdPersonViewLocation()`** (line 411)
   - Calculates where camera should be positioned
   - Positions camera behind player using `cg_thirdPersonRange` (distance)
   - Moves camera backwards from target along view forward vector (`camerafwd`)

3. **`CG_UpdateThirdPersonTargetDamp()`** (line 510)
   - Smoothly interpolates the camera target position
   - Uses damping factor `cg_thirdPersonTargetDamp` for smooth following
   - Performs collision traces to ensure target is valid

4. **`CG_UpdateThirdPersonCameraDamp()`** (line 573)
   - Smoothly interpolates the camera location
   - Uses damping factor `cg_thirdPersonCameraDamp` for smooth movement
   - Adjusts damping based on camera pitch angle
   - Performs collision traces to prevent camera clipping through walls

**Camera Flow**:
```
Player viewangles (from input)
    ↓
cameraFocusAngles (copy of player view)
    ↓
Calculate camerafwd vector (from focus angles)
    ↓
Calculate cameraIdealTarget (slightly above player head)
    ↓
Calculate cameraIdealLoc (behind player, at distance)
    ↓
Apply damping/smoothing to cameraCurTarget and cameraCurLoc
    ↓
Trace for collisions, adjust if needed
    ↓
Calculate final view angles: vectoangles(cameraCurTarget - cameraCurLoc)
    ↓
Set cg.refdef.vieworg and cg.refdefViewAngles
```

**Key CVars**:
- `cg_thirdPersonRange` - Distance of camera behind player
- `cg_thirdPersonAngle` - Horizontal angle offset
- `cg_thirdPersonVertOffset` - Vertical offset for camera target
- `cg_thirdPersonCameraDamp` - Camera position smoothing
- `cg_thirdPersonTargetDamp` - Target position smoothing

---

## Our Implementation (Jank/OpenGL)

### Architecture Overview

Our system uses an **event-sourced, immediate-mode architecture**:

```
┌──────────────────┐
│   Main Loop      │
│                  │
│ 1. Update Time   │  Calculate delta-time
│ 2. Update Cursor │  Get mouse position, calculate offsets
│ 3. Process Input │  Generate command from input state
│ 4. Handle Command│  Convert command → events
│ 5. Append Events │  Add to event store
│ 6. Reduce State  │  Fold all events into state
│ 7. Render        │  Draw current state
└──────────────────┘
```

### Component Breakdown

#### 1. Input Collection (`src/app/core.jank`)

**Keyboard Input** - `process-input` (line 31):
```clojure
(defn process-input [{:keys [window delta-time cursor/pitch cursor/yaw]}]
  (cond-> {:command/name :player/process-input
           :delta-time delta-time
           :pitch pitch
           :yaw yaw}
    (= (cpp/glfwGetKey window GLFW_KEY_W) GLFW_PRESS)
    (assoc :forward true)
    ;; ... etc for other keys
```

**Mouse Input** - `update-cursor` (line 149):
```clojure
(defn update-cursor
  [{:cursor/keys [last-x last-y sensitivity yaw pitch initialized?]
    :keys [window]}]
  (let [cursor-x (cpp/double)
        cursor-y (cpp/double)
        _ (cpp/glfwGetCursorPos window (cpp/& cursor-x) (cpp/& cursor-y))

        x-offset (cpp/* (cpp/- cursor-x cursor-last-x) cursor-sensitivity)
        y-offset (cpp/* (cpp/- cursor-last-y cursor-y) cursor-sensitivity)

        cursor-yaw (math/*-> :float yaw)
        cursor-pitch (math/*-> :float pitch)]

    (cpp/= cursor-yaw (cpp/+ cursor-yaw x-offset))
    (cpp/= cursor-pitch (cpp/+ cursor-pitch y-offset))

    ;; Clamp pitch to ±89 degrees
    (when (cpp/> cursor-pitch (cpp/float 89.0))
      (cpp/= cursor-pitch (cpp/float 89.0)))
    (when (cpp/< cursor-pitch (cpp/float -89.0))
      (cpp/= cursor-pitch (cpp/float -89.0)))))
```

#### 2. Command → Event → State Flow

**Command Generation** (line 31):
- `process-input` reads current input state
- Returns command map with pressed keys and current pitch/yaw

**Event Emission** (line 281):
```clojure
(defmethod handle-command :player/process-input
  [{:keys [command]}]
  (let [{:keys [exit forward backward left right]} command]
    (cond
      exit [{:event/type :system/exit}]
      (or forward backward left right)
      [(-> command
           (dissoc :command/name)
           (assoc :event/type :player/moved))])))
```

**State Reduction** (line 222):
```clojure
(defmethod handle-event :player/moved
  [{:keys [entities] :as state}
   {{:keys [id forward backward left right yaw pitch delta-time]} :event}]
  (let [player (get entities id)
        movespeed (cpp/* (cpp/float 5.0) (cpp/float delta-time))
        yaw-radians (cpp/glm.radians (cpp/float yaw))
        front (math/gimmie :vec3 [(cpp/cos yaw-radians) 0.0 (cpp/sin yaw-radians)])
        ;; ... movement calculations
        ]
    (-> state
        (update-in [:entities id] assoc
                   :transform/position [x y z]
                   :transform/pitch pitch
                   :transform/yaw yaw))))
```

#### 3. Camera System (`src/app/core.jank`)

**Camera Setup** - `setup-camera` (line 315):
```clojure
(defn setup-camera []
  (let [camera-position (math/gimmie :boxed :vec3 [0.0 1.0 3.0])
        camera-front (math/gimmie :boxed :vec3 [0.0 0.0 -1.0])]
    {:position camera-position
     :front camera-front
     ;; ...
     }))
```

**Camera Update** - `update-camera` (line 101):
```clojure
(defn update-camera
  [{{:keys [front]} :camera :cursor/keys [pitch yaw]}]
  (let [direction (math/gimmie :vec3 [0.0 0.0 0.0])
        ;; Calculate direction vector from pitch/yaw
        _ (cpp/= (cpp/aget direction 0)
                 (cpp/* (cpp/cos (cpp/glm.radians yaw))
                        (cpp/cos (cpp/glm.radians pitch))))
        _ (cpp/= (cpp/aget direction 1)
                 (cpp/sin (cpp/glm.radians pitch)))
        _ (cpp/= (cpp/aget direction 2)
                 (cpp/* (cpp/sin (cpp/glm.radians yaw))
                        (cpp/cos (cpp/glm.radians pitch))))
        _ (cpp/= (math/*-> :vec3 front) direction)]))
```

**View Matrix** - `camera-system` (line 117):
```clojure
(defn camera-system [{{:keys [front position]} :camera ...}]
  (let [camera-position (math/*-> :vec3 position)
        camera-front (math/*-> :vec3 front)
        camera-up (cpp/glm.vec3 0.0 1.0 0.0)

        view-m (cpp/glm.lookAt camera-position
                               (cpp/+ camera-position camera-front)
                               camera-up)
        ;; ... set uniform
        ]))
```

---

## Detailed Comparison

| Aspect | Jedi Academy | Our Implementation |
|--------|-------------|---------------------|
| **Architecture** | Client-server predictive with authoritative validation | Event-sourced, single-context |
| **Input Collection** | `CL_MouseEvent()` accumulates deltas in arrays | `update-cursor` directly reads cursor position via `glfwGetCursorPos` |
| **Angle Storage** | `cl.viewangles[]` (client floats) + `ps->viewangles[]` (server authoritative) | `:cursor/pitch`, `:cursor/yaw` (boxed floats in context) + `:transform/pitch`, `:transform/yaw` (in entity state) |
| **Angle Updates** | Incremental: `cl.viewangles[YAW] -= m_yaw * mx` | Direct calculation: `(cpp/= cursor-yaw (cpp/+ cursor-yaw x-offset))` |
| **Pitch Clamping** | Server-side in `PM_UpdateViewAngles()` with context-specific limits | Client-side immediate in `update-cursor` (±89°) |
| **Sensitivity** | Multi-layered: `cl_sensitivity` × `cl_mouseAccel` × `FOV scale` | Single: `:cursor/sensitivity` (0.05) |
| **Movement Direction** | Calculated server-side from `ps->viewangles` | Calculated client-side from yaw in `:player/moved` handler |
| **Camera System** | **Separate** third-person camera with damping, collision detection | **Direct** first-person: camera uses player's front vector |
| **State Management** | Predictive: client simulates, server corrects | Event-sourced: commands → events → state reduction |
| **Network Precision** | Floats (client) → shorts (network) → floats (server) | N/A (single-player) |
| **Delta Angles** | Server can force view changes via `ps->delta_angles[]` | N/A |

---

## Key Architectural Differences

### 1. Stateful vs Event-Driven

**Jedi Academy**: Stateful mutation
```c
// Direct mutation
cl.viewangles[YAW] -= m_yaw->value * mx;
cl.viewangles[PITCH] += m_pitch->value * my;
```

**Our Code**: Event-sourced reduction
```clojure
;; Command → Event → State transformation
(defmethod handle-event :player/moved
  [{:keys [entities] :as state} {{:keys [yaw pitch]} :event}]
  (update-in state [:entities id] assoc
    :transform/pitch pitch
    :transform/yaw yaw))
```

### 2. Client-Server vs Single-Context

**Jedi Academy**:
- Client predicts movement
- Server validates angles (`PM_UpdateViewAngles`)
- Delta angles allow server overrides
- Network-efficient (shorts, delta compression)

**Our Code**:
- Single context (no networking)
- All validation happens immediately
- Events stored for replay/debugging
- State is pure (no side effects in reduction)

### 3. Input Processing

**Jedi Academy**: Multi-stage pipeline
```
Raw Input → Accumulate in cl.mouseDx/Dy
         → Apply in CL_MouseMove (with filtering, accel)
         → Clamp in CL_CreateCmd
         → Convert to shorts
         → Send to server
         → Reconstruct and validate
```

**Our Code**: Direct pipeline
```
Raw Cursor → Calculate offset from last position (update-cursor)
          → Generate command (process-input)
          → Handle command → Event
          → Reduce event into state
          → Render
```

### 4. Camera Implementation

**Jedi Academy**: Sophisticated third-person
- Camera follows player with damping (`CG_UpdateThirdPersonCameraDamp`)
- Smooth target tracking (`CG_UpdateThirdPersonTargetDamp`)
- Collision detection (traces prevent wall clipping)
- Separate camera angles from player angles

**Our Code**: Simple first-person
```clojure
;; Camera directly uses player's front vector
(defn update-camera [{{:keys [front]} :camera :cursor/keys [pitch yaw]}]
  (let [direction (math/gimmie :vec3 [0.0 0.0 0.0])
        ;; Calculate direction from pitch/yaw
        _ (cpp/= (cpp/aget direction 0)
                 (cpp/* (cpp/cos (cpp/glm.radians yaw))
                        (cpp/cos (cpp/glm.radians pitch))))
        ;; ...
        _ (cpp/= (math/*-> :vec3 front) direction)]))

;; View matrix uses player position + front
(cpp/glm.lookAt camera-position
                (cpp/+ camera-position camera-front)
                camera-up)
```

---

## Strengths & Weaknesses

### Jedi Academy Strengths:
✅ Network-ready with prediction and validation
✅ Delta angles enable complex interactions (vehicles, scripted events)
✅ Sophisticated camera with smoothing and collision
✅ Context-sensitive angle limits (emplaced weapons, vehicles)
✅ Proven stable in production

### Jedi Academy Weaknesses:
❌ Complex: Multiple layers of indirection
❌ Harder to debug (client-server desync issues)
❌ Stateful mutations throughout
❌ Tightly coupled to networking concerns

### Our Implementation Strengths:
✅ **Event-sourced**: Full history, easy debugging, time travel
✅ **Pure state transformations**: No hidden mutations
✅ **Simpler**: Direct relationship between input and output
✅ **Testable**: Events are data, easy to test reducers
✅ **Decoupled**: Clear separation via events

### Our Implementation Weaknesses:
❌ No smoothing/damping (will feel jerky)
❌ Direct cursor position (should use deltas like JA)
❌ No camera collision detection
❌ Single-player only (needs rework for networking)
❌ Pitch/yaw stored in two places (context + entity state)

---

## Recommendations

### 1. Use Mouse Deltas, Not Absolute Position

**Current (problematic)**:
```clojure
x-offset (cpp/* (cpp/- cursor-x cursor-last-x) cursor-sensitivity)
```

This works but requires tracking `last-x/last-y`. Better approach:

```clojure
;; In setup, register callback
(cpp/glfwSetCursorPosCallback window
  (fn [window xpos ypos]
    (swap! mouse-deltas conj {:dx (- xpos @last-x)
                               :dy (- ypos @last-y)})
    (reset! last-x xpos)
    (reset! last-y ypos)))

;; In update, consume accumulated deltas
(defn process-mouse-deltas [deltas sensitivity]
  (let [total-dx (reduce + (map :dx deltas))
        total-dy (reduce + (map :dy deltas))]
    {:yaw-delta (* total-dx sensitivity)
     :pitch-delta (* total-dy sensitivity)}))
```

### 2. Separate View State from Entity State

**Current issue**: `:cursor/pitch` in context AND `:transform/pitch` in entity

**Suggested**:
```clojure
;; View state (camera-relative, updated every frame)
{:view/pitch 0.0
 :view/yaw -90.0
 :view/roll 0.0}

;; Entity state (world-space, updated by events)
{:entities
  {player-id
    {:transform/position [x y z]
     :transform/rotation [pitch yaw roll]  ; World-space orientation
     :transform/forward-vector [...]}}}    ; Derived from rotation
```

### 3. Add Smoothing/Damping

For camera following (if you add third-person):

```clojure
(defn lerp [a b t]
  (+ a (* t (- b a))))

(defn smooth-camera-follow
  [{:keys [camera-pos target-pos damping delta-time]}]
  (let [t (- 1.0 (Math/pow damping delta-time))]
    {:camera-pos [(lerp (camera-pos 0) (target-pos 0) t)
                  (lerp (camera-pos 1) (target-pos 1) t)
                  (lerp (camera-pos 2) (target-pos 2) t)]}))
```

### 4. Consider Keeping Event Sourcing

Your event-sourced architecture is actually a **strength** for a different use case:
- Perfect for replay systems
- Great for debugging (see full input history)
- Enables time-travel debugging
- Can add "undo" functionality easily

Don't abandon it to copy Jedi Academy's architecture—instead, **adapt the input handling techniques** while keeping your event model.

### 5. Improve Movement from View Direction

Your movement calculation is correct but could be clearer:

```clojure
;; Current (correct but terse)
front (math/gimmie :vec3 [(cpp/cos yaw-radians) 0.0 (cpp/sin yaw-radians)])

;; Clearer with comments
;; Calculate horizontal forward vector (y=0 means ground-plane only)
;; This gives strafe-relative movement like JA
(let [yaw-rad (cpp/glm.radians yaw)
      forward-vec (math/gimmie :vec3
                    [(cpp/cos yaw-rad)  ; X component
                     0.0                 ; Y (no vertical component)
                     (cpp/sin yaw-rad)]) ; Z component
      right-vec (cpp/glm.normalize
                 (cpp/glm.cross forward-vec up-vec))]
  ;; Use forward-vec and right-vec for WASD movement
  )
```

### 6. Add Camera Collision Detection

If adding third-person view:

```clojure
(defn trace-camera-collision
  [{:keys [start-pos end-pos world-geometry]}]
  ;; Ray trace from player to desired camera position
  ;; If hit, place camera at collision point
  ;; Prevents camera going through walls
  )
```

---

## Summary

**Jedi Academy** uses a battle-tested, network-ready architecture with client prediction and server validation. It's complex but handles edge cases (vehicles, forced camera angles, network latency) elegantly.

**Our implementation** is cleaner and more functional, with event sourcing enabling powerful debugging and replay. However, it needs:

1. ✅ Delta-based mouse input (not absolute positions)
2. ✅ Separation of view state from entity state
3. ✅ Optional smoothing/damping for polish
4. ✅ Camera collision detection if adding third-person

**The good news**: Your event-sourced architecture is fundamentally sound. You can add Jedi Academy's input handling techniques while keeping your clean, functional approach. **Don't throw away your event store—it's a feature, not a bug!**

---

## References

### Jedi Academy Source Files

- **Input**: `code/client/cl_input.cpp`
- **View Angles**: `code/game/bg_pangles.cpp` (PM_UpdateViewAngles)
- **Player Movement**: `code/game/bg_pmove.cpp`
- **Camera**: `code/cgame/cg_view.cpp`

### Our Source Files

- **Main**: `src/app/core.jank`
- **Math**: `src/app/math/core.jank`
- **Event Store**: `src/app/event_store/core.jank`

### Key Concepts

- **Delta Angles**: Server-side offset allowing forced view changes
- **Predictive Movement**: Client simulates, server validates
- **Event Sourcing**: Commands → Events → State reduction
- **Third-Person Camera**: Separate system from character rotation
- **Damping/Smoothing**: Exponential interpolation for smooth camera movement
