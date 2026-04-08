# ShaTV Recent History Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 ShaTV 增加最多 `5` 条的最近打开记录，并通过 `File -> Open Recent` 重新打开，不自动恢复播放。

**Architecture:** 扩展 `AppSettings` 作为最近记录的持久化入口，继续复用单个 `config.toml`。`Application` 负责在现有打开流程上记录历史并转发最近记录点击事件，`MainWindow` 只负责渲染和发信号。

**Tech Stack:** C++20, Qt6 Widgets, QtTest, toml11

---

## File Structure

**Modify**

- `src/app/app_settings.h`
- `src/app/app_settings.cpp`
- `src/app/application.h`
- `src/app/application.cpp`
- `src/ui/windows/main_window.h`
- `src/ui/windows/main_window.cpp`
- `tests/unit/launch_options_test.cpp`
- `translations/shatv_zh_CN.ts`
- `README.md`

---

### Task 1: Add Recent History Persistence

**Files:**

- Modify: `tests/unit/launch_options_test.cpp`
- Modify: `src/app/app_settings.h`
- Modify: `src/app/app_settings.cpp`

- [ ] **Step 1: Write the failing settings test**

Add one test method to `tests/unit/launch_options_test.cpp` that:

- creates a temporary `config.toml`
- pushes more than `5` recent entries through `AppSettings`
- repeats one existing entry to verify de-duplication
- saves and reloads settings
- asserts:
  - only `5` entries remain
  - newest entry is first
  - duplicate target appears only once

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
cmake -S . -B build-tests -DBUILD_TESTING=ON
cmake --build build-tests --target launch_options_test
./build-tests/tests/launch_options_test
```

Expected: FAIL because `AppSettings` does not yet expose recent-history APIs.

- [ ] **Step 3: Add the minimal persistence model**

Extend `src/app/app_settings.h` with:

- a small `RecentOpenItem` struct
- getter for recent items
- mutator for recording one item

Implement in `src/app/app_settings.cpp`:

- TOML load/save for `[[history.recent]]`
- de-duplication by `(kind, target)`
- hard cap of `5`

- [ ] **Step 4: Run the test to verify it passes**

Run:

```bash
cmake --build build-tests --target launch_options_test
./build-tests/tests/launch_options_test
```

Expected: PASS

---

### Task 2: Wire History Through Application

**Files:**

- Modify: `src/app/application.h`
- Modify: `src/app/application.cpp`

- [ ] **Step 1: Add the failing integration path locally**

Before implementation, identify the exact open paths that must call the history writer:

- `OpenFile()`
- `OpenPlaylistFile()`
- `OpenUrl()`
- startup paths from `--open-media` / `--open-url`

Expected failing behavior: records are never shown or updated because `Application` never pushes them into `AppSettings` or `MainWindow`.

- [ ] **Step 2: Implement minimal application helpers**

Add helpers in `Application` to:

- build `RecentOpenItem`
- record it through `AppSettings`
- refresh recent menu data in `MainWindow`

Rules:

- file opens record as `kind = "file"`
- URL opens record as `kind = "url"`
- smoke paths do not record

- [ ] **Step 3: Reuse existing open logic for recent items**

Add a new signal from `MainWindow` that carries the selected recent item.

`Application` should respond by calling existing methods:

- file -> `OpenFile(...)`
- url -> `OpenUrl(...)`

- [ ] **Step 4: Run app-level verification**

Run:

```bash
cmake --build build --target shatv
```

Expected: build succeeds with no new target required.

---

### Task 3: Add the Open Recent Menu

**Files:**

- Modify: `src/ui/windows/main_window.h`
- Modify: `src/ui/windows/main_window.cpp`
- Modify: `translations/shatv_zh_CN.ts`

- [ ] **Step 1: Add menu plumbing**

Add to `MainWindow`:

- a `QMenu *recent_menu_`
- a method to receive recent items and rebuild menu actions
- a signal for “recent item selected”

- [ ] **Step 2: Implement menu behavior**

In `BuildUi()`:

- create `Open Recent` submenu under `File`
- disable it when empty
- rebuild actions when recent items change
- store item metadata on `QAction`

- [ ] **Step 3: Add translations**

Add only the new strings required for this feature:

- `Open Recent`
- save-failure status message if one is shown in status bar

- [ ] **Step 4: Manual verification**

Run:

```bash
./build/src/shatv
```

Then verify:

1. open one URL
2. restart app
3. `File -> Open Recent` shows the URL label
4. clicking it reopens the source

---

### Task 4: Final Verification

**Files:**

- Modify if needed: `README.md`

- [ ] **Step 1: Rebuild tests and app**

Run:

```bash
cmake -S . -B build-tests -DBUILD_TESTING=ON
cmake --build build-tests
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build --target shatv
```

- [ ] **Step 2: Run automated tests**

Run:

```bash
ctest --test-dir build-tests -V
```

Expected: all existing tests pass.

- [ ] **Step 3: Manual recent-history check**

Verify:

```bash
./build/src/shatv
```

Checklist:

- opening a file records it
- opening a URL records it
- repeated opens move the item to the top
- list never exceeds `5`
- app startup still does not auto-play
