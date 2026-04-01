# Stage 3 libmpv Backend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 `ShaTV` 接入真实 `libmpv` 播放后端，用 `QOpenGLWidget + libmpv render API` 替换占位视频区，并完成最小状态回流与自动重试闭环。

**Architecture:** 保持现有 `ui -> application -> player` 单向依赖不变。`Application` 在装配层选择真实后端或 `FakePlayerBackend`，但在 `Task 2` 完成前默认运行路径继续保留 `FakePlayerBackend`，只用 `--mpv-smoke` 驱动真实后端，避免中间态回归。`MpvPlayerBackend` 封装 `mpv_handle/mpv_render_context`，`MpvRenderWidget` 只负责 OpenGL 生命周期和 render 入口，render update callback 只通过 queued connection 请求重绘，不直接在回调里执行渲染；`PlayerController` 负责最小错误重试策略。

**Tech Stack:** C++20, Qt6 Core/Widgets/OpenGLWidgets/Test, libmpv (`client.h`, `render.h`, `render_gl.h`), CMake, QtTest

---

## File Structure

**Create:**

- `src/player/mpv_player_backend.h`
- `src/player/mpv_player_backend.cpp`
- `src/player/mpv_event_adapter.h`
- `src/player/mpv_event_adapter.cpp`
- `tests/unit/mpv_event_adapter_test.cpp`
- `tests/unit/player_controller_retry_test.cpp`

**Modify:**

- `CMakeLists.txt`
- `src/CMakeLists.txt`
- `src/app/application.h`
- `src/app/application.cpp`
- `src/application/player_controller.h`
- `src/application/player_controller.cpp`
- `src/domain/playback_state.h`
- `src/domain/player_snapshot.h`
- `src/player/player_backend.h`
- `src/player/mpv_render_widget.h`
- `src/player/mpv_render_widget.cpp`
- `src/ui/windows/main_window.h`
- `src/ui/windows/main_window.cpp`
- `src/ui/panels/playback_status_panel.cpp`
- `tests/CMakeLists.txt`
- `TODO.md`

**Keep unchanged in Stage 3:**

- `src/ui/models/channel_list_model.cpp`

说明：

- `MainWindow` 继续只消费 `PlayerController` 归一化后的快照。
- `MpvPlayerBackend` 不直接操作 UI。
- `Application` 负责把 `MpvRenderWidget` 绑定给 `MpvPlayerBackend`。

### Task 1: Build Plumbing And Backend Selection

**Files:**

- Create: `src/player/mpv_player_backend.h`
- Create: `src/player/mpv_player_backend.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/CMakeLists.txt`
- Modify: `src/app/application.h`
- Modify: `src/app/application.cpp`
- Test: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing smoke expectation**

在 `tests/CMakeLists.txt` 中新增一个可选的真实后端 smoke 测试，只有显式提供媒体路径时才启用，避免 CI 依赖网络。

```cmake
if(DEFINED ENV{SHATV_SMOKE_MEDIA} AND NOT "$ENV{SHATV_SMOKE_MEDIA}" STREQUAL "")
    add_test(
        NAME shatv_mpv_smoke
        COMMAND ${CMAKE_COMMAND} -E env
            QT_QPA_PLATFORM=offscreen
            SHATV_SMOKE_MEDIA=$ENV{SHATV_SMOKE_MEDIA}
            $<TARGET_FILE:shatv> --mpv-smoke
    )

    set_tests_properties(
        shatv_mpv_smoke
        PROPERTIES PASS_REGULAR_EXPRESSION "ShaTV Stage3 mpv smoke ok state=playing"
    )
endif()
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cmake -S . -B build
ctest --test-dir build -V -R shatv_mpv_smoke
```

Expected:

- 设置或修改 `SHATV_SMOKE_MEDIA` 后，必须重新执行 `cmake -S . -B build`，因为测试注册发生在 configure 阶段
- 如果未设置 `SHATV_SMOKE_MEDIA`，此测试不会被注册
- 如果设置了 `SHATV_SMOKE_MEDIA`，测试应失败，因为当前还没有 `--mpv-smoke` 和 `MpvPlayerBackend`

- [ ] **Step 3: Wire mpv into CMake**

在顶层和 `src/CMakeLists.txt` 中引入 OpenGLWidgets 和 `mpv`：

```cmake
# CMakeLists.txt
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
```

```cmake
# src/CMakeLists.txt
add_library(shatv_app
    app/application.cpp
    application/player_controller.cpp
    player/fake_player_backend.cpp
    player/mpv_player_backend.cpp
    player/mpv_event_adapter.cpp
    player/mpv_render_widget.cpp
    player/player_backend.cpp
    ui/models/channel_list_model.cpp
    ui/panels/player_control_bar.cpp
    ui/panels/playback_status_panel.cpp
    ui/windows/main_window.cpp
)

target_link_libraries(shatv_app
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGLWidgets
        PkgConfig::MPV
)
```

- [ ] **Step 4: Make `Application` own a polymorphic backend**

把 `Application` 从持有 `std::unique_ptr<FakePlayerBackend>` 改成持有 `std::unique_ptr<PlayerBackend>`，但在 render 还未接通前，不要改变普通运行路径，先只让 `--mpv-smoke` 使用真实后端：

```cpp
// src/app/application.h
std::unique_ptr<player::PlayerBackend> backend_;
```

```cpp
// src/app/application.cpp
const bool mpv_smoke = qt_app_->arguments().contains("--mpv-smoke");
if (mpv_smoke) {
    backend_ = std::make_unique<player::MpvPlayerBackend>();
} else {
    backend_ = std::make_unique<player::FakePlayerBackend>();
}

controller_ = std::make_unique<application::PlayerController>(backend_.get());
```

- [ ] **Step 5: Add a minimal `MpvPlayerBackend` class skeleton**

先只提供可编译骨架，不做真实渲染：

```cpp
// src/player/mpv_player_backend.h
class MpvPlayerBackend final : public PlayerBackend {
    Q_OBJECT

public:
    explicit MpvPlayerBackend(QObject *parent = nullptr);
    ~MpvPlayerBackend() override;

    void Load(const domain::Channel &channel) override;
    void Play() override;
    void Pause() override;
    void Stop() override;
    void SetVolume(int volume) override;
    void SetMuted(bool muted) override;

private:
    mpv_handle *handle_ = nullptr;
};
```

- [ ] **Step 6: Run build to verify it passes**

Run:

```bash
cmake -S . -B build
cmake --build build
```

Expected:

- `Built target shatv`
- `Built target shatv_unit_test`

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/CMakeLists.txt src/app src/player tests/CMakeLists.txt
git commit -m "feat: add mpv backend build plumbing"
```

### Task 2: Replace Placeholder Rendering With OpenGL Render API

**Files:**

- Modify: `src/player/mpv_render_widget.h`
- Modify: `src/player/mpv_render_widget.cpp`
- Modify: `src/player/mpv_player_backend.h`
- Modify: `src/player/mpv_player_backend.cpp`
- Modify: `src/app/application.cpp`
- Modify: `src/ui/windows/main_window.h`
- Modify: `src/ui/windows/main_window.cpp`

- [ ] **Step 1: Reuse `shatv_mpv_smoke` as the RED gate**

继续使用 `Task 1` 注册的 `shatv_mpv_smoke` 作为自动化 RED/GREEN 门槛，不再新增只打印日志的手工“测试”。

- [ ] **Step 2: Run smoke test to verify it still fails before render integration**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build -V -R shatv_mpv_smoke
```

Expected:

- 失败原因应明确落在“真实后端已选中，但还没有 render widget / render context / 绑定路径”

- [ ] **Step 3: Convert `MpvRenderWidget` to `QOpenGLWidget`**

把占位 `QWidget` 改成 `QOpenGLWidget`，但仍保留必要的占位文字：

```cpp
class MpvRenderWidget final : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit MpvRenderWidget(QWidget *parent = nullptr);
    void SetBackend(class MpvPlayerBackend *backend);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    MpvPlayerBackend *backend_ = nullptr;
};
```

- [ ] **Step 4: Create and use `mpv_render_context`**

在 `MpvPlayerBackend` 中增加 OpenGL render API 所需成员：

```cpp
mpv_handle *handle_ = nullptr;
mpv_render_context *render_context_ = nullptr;
MpvRenderWidget *render_widget_ = nullptr;
```

初始化关键调用：

```cpp
mpv_opengl_init_params gl_init{
    .get_proc_address = GetProcAddress,
    .get_proc_address_ctx = nullptr,
};

mpv_render_param params[] = {
    {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
    {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
    {MPV_RENDER_PARAM_INVALID, nullptr},
};

mpv_render_context_create(&render_context_, handle_, params);
mpv_render_context_set_update_callback(render_context_, &MpvPlayerBackend::OnRenderUpdate, this);
```

渲染调用：

```cpp
mpv_opengl_fbo fbo{
    .fbo = static_cast<int>(defaultFramebufferObject()),
    .w = width() * devicePixelRatio(),
    .h = height() * devicePixelRatio(),
    .internal_format = 0,
};

int flip_y = 1;
mpv_render_param render_params[] = {
    {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
    {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
    {MPV_RENDER_PARAM_INVALID, nullptr},
};

mpv_render_context_render(render_context_, render_params);
```

- [ ] **Step 5: Make render updates thread-safe**

update callback 不直接调用 render API，只负责把重绘请求投递回 Qt GUI 线程：

```cpp
static void OnRenderUpdate(void *ctx) {
    auto *self = static_cast<MpvPlayerBackend *>(ctx);
    QMetaObject::invokeMethod(self, &MpvPlayerBackend::RequestRenderUpdate, Qt::QueuedConnection);
}

void MpvPlayerBackend::RequestRenderUpdate() {
    if (render_widget_ != nullptr) {
        render_widget_->update();
    }
}
```

说明：

- 不要在 `OnRenderUpdate` 里直接调用 `mpv_render_context_render()`
- `paintGL()` 才是实际 render 调用入口

- [ ] **Step 6: Bind backend and widget in the app assembly layer**

不要让 `PlayerController` 知道 widget。绑定应留在 `Application`：

```cpp
if (auto *mpv_backend = dynamic_cast<player::MpvPlayerBackend *>(backend_.get())) {
    mpv_backend->AttachRenderWidget(main_window_->RenderWidget());
    main_window_->RenderWidget()->SetBackend(mpv_backend);
}
```

需要为 `MainWindow` 增加只读 getter：

```cpp
player::MpvRenderWidget *RenderWidget() const;
```

- [ ] **Step 7: Promote normal runtime to the real backend only after render path works**

当 `MpvRenderWidget`、`MpvPlayerBackend` 和绑定路径都已接通后，再切换普通运行路径：

```cpp
const bool mpv_smoke = qt_app_->arguments().contains("--mpv-smoke");
if (smoke_test_) {
    backend_ = std::make_unique<player::FakePlayerBackend>();
} else {
    backend_ = std::make_unique<player::MpvPlayerBackend>();
}
```

要求：

- `--smoke-test` 继续保留阶段 2 的 `FakePlayerBackend`
- `--mpv-smoke` 和普通桌面运行都使用真实后端

- [ ] **Step 8: Run build and desktop smoke verification**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build -V -R shatv_mpv_smoke
env QT_QPA_PLATFORM=offscreen SHATV_SMOKE_MEDIA=/absolute/path/to/local.mp4 ./build/src/shatv --mpv-smoke
```

Expected:

- build 成功
- `shatv_mpv_smoke` PASS
- 如果本地媒体有效，`--mpv-smoke` 输出 `ShaTV Stage3 mpv smoke ok state=playing`

- [ ] **Step 9: Commit**

```bash
git add src/app src/player src/ui/windows
git commit -m "feat: add mpv render widget integration"
```

### Task 3: Add mpv Event Mapping And UI State Backflow

**Files:**

- Create: `src/player/mpv_event_adapter.h`
- Create: `src/player/mpv_event_adapter.cpp`
- Create: `tests/unit/mpv_event_adapter_test.cpp`
- Modify: `src/domain/playback_state.h`
- Modify: `src/domain/player_snapshot.h`
- Modify: `src/player/mpv_player_backend.cpp`
- Modify: `src/ui/panels/playback_status_panel.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing adapter tests**

为原始 mpv 事件到 `PlayerSnapshot` 的映射增加单测。至少覆盖：

- `FILE_LOADED -> Playing`
- `pause=true -> Paused`
- `END_FILE(error) -> Error`

```cpp
void MpvEventAdapterTest::file_loaded_becomes_playing() {
    MpvEventAdapter adapter;
    PlayerSnapshot snapshot{};

    adapter.ApplyFileLoaded(snapshot, "Demo News");

    QCOMPARE(snapshot.state, PlaybackState::kPlaying);
    QCOMPARE(snapshot.channel_name, QString("Demo News"));
}
```

- [ ] **Step 2: Run the new test to verify it fails**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build -V -R mpv_event_adapter_test
```

Expected:

- FAIL，因为 `MpvEventAdapter` 还不存在

- [ ] **Step 3: Extend snapshot/state types for real backend**

在 `PlaybackState` 中补足阶段 3 需要的状态：

```cpp
enum class PlaybackState : uint8_t {
    kIdle = 0,
    kLoading,
    kPlaying,
    kPaused,
    kBuffering,
    kRetrying,
    kError,
};
```

在 `PlayerSnapshot` 中增加最小错误信息：

```cpp
struct PlayerSnapshot {
    PlaybackState state = PlaybackState::kIdle;
    QString channel_id;
    QString channel_name;
    QString message;
    int volume = 50;
    bool muted = false;
    int retry_count = 0;
};
```

- [ ] **Step 4: Implement a pure event adapter**

`MpvEventAdapter` 只做“状态归一化”，不直接操作 Qt 控件：

```cpp
class MpvEventAdapter {
public:
    void ApplyFileLoaded(domain::PlayerSnapshot &snapshot, const QString &channel_name) const;
    void ApplyPauseChanged(domain::PlayerSnapshot &snapshot, bool paused) const;
    void ApplyEndFileError(domain::PlayerSnapshot &snapshot, const QString &message) const;
};
```

- [ ] **Step 5: Drain mpv events and update snapshots in `MpvPlayerBackend`**

事件循环建议使用 `mpv_set_wakeup_callback()` + `mpv_wait_event(handle_, 0)`：

```cpp
static void Wakeup(void *ctx) {
    auto *self = static_cast<MpvPlayerBackend *>(ctx);
    QMetaObject::invokeMethod(self, &MpvPlayerBackend::DrainEvents, Qt::QueuedConnection);
}

void MpvPlayerBackend::DrainEvents() {
    while (true) {
        mpv_event *event = mpv_wait_event(handle_, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        HandleEvent(event);
    }
}
```

观察最小必要属性：

```cpp
mpv_observe_property(handle_, 0, "pause", MPV_FORMAT_FLAG);
mpv_observe_property(handle_, 0, "media-title", MPV_FORMAT_STRING);
mpv_observe_property(handle_, 0, "idle-active", MPV_FORMAT_FLAG);
```

- [ ] **Step 6: Reflect snapshot changes in the status panel**

状态面板显示新增状态和重试次数：

```cpp
message_value_label_->setText(
    snapshot.retry_count > 0
        ? QString("%1 (retry %2)").arg(snapshot.message).arg(snapshot.retry_count)
        : snapshot.message
);
```

- [ ] **Step 7: Run tests to verify they pass**

Run:

```bash
cmake --build build
ctest --test-dir build -V -R "mpv_event_adapter_test|shatv_unit_test|shatv_smoke"
```

Expected:

- 相关单测全部 PASS
- 现有阶段 2 smoke 不回归

- [ ] **Step 8: Commit**

```bash
git add src/domain src/player src/ui/panels tests
git commit -m "feat: map mpv events to player snapshots"
```

### Task 4: Add Minimal Auto-Retry And Final Verification

**Files:**

- Create: `tests/unit/player_controller_retry_test.cpp`
- Modify: `src/application/player_controller.h`
- Modify: `src/application/player_controller.cpp`
- Modify: `src/player/mpv_player_backend.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `TODO.md`

- [ ] **Step 1: Write the failing retry test**

用测试替身验证控制器在错误时会触发一次最小重试：

```cpp
void PlayerControllerRetryTest::error_triggers_single_retry() {
    ScriptedBackend backend;
    PlayerController controller(&backend);

    controller.PlayChannel(MakeChannel("demo-news"));
    backend.EmitError("Network timeout");

    QTRY_COMPARE(backend.load_count(), 2);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cmake --build build
ctest --test-dir build -V -R player_controller_retry_test
```

Expected:

- FAIL，因为当前 `PlayerController` 还没有重试策略

- [ ] **Step 3: Implement a single bounded retry in `PlayerController`**

增加最小状态：

```cpp
int retry_count_ = 0;
static constexpr int kMaxRetryCount = 1;
```

在错误快照到达时触发重试，并补上重试计数的重置时机：

```cpp
void PlayerController::PlayChannel(const domain::Channel &channel) {
    current_channel_ = channel;
    retry_count_ = 0;
    current_snapshot_.retry_count = 0;
    backend_->Load(channel);
}

void PlayerController::OnBackendSnapshotChanged(const domain::PlayerSnapshot &snapshot) {
    if (snapshot.state == domain::PlaybackState::kPlaying) {
        retry_count_ = 0;
    }

    if (snapshot.state == domain::PlaybackState::kError && retry_count_ < kMaxRetryCount) {
        ++retry_count_;
        current_snapshot_ = snapshot;
        current_snapshot_.state = domain::PlaybackState::kRetrying;
        current_snapshot_.retry_count = retry_count_;
        emit PlaybackSnapshotChanged(current_snapshot_);

        QTimer::singleShot(300, this, [this]() { backend_->Load(current_channel_); });
        return;
    }
}
```

- [ ] **Step 4: Make `MpvPlayerBackend` emit an explicit error snapshot**

在 `MPV_EVENT_END_FILE` 和关键命令错误时构造：

```cpp
snapshot.state = domain::PlaybackState::kError;
snapshot.message = QString("mpv end-file error: %1").arg(reason_text);
emit SnapshotChanged(snapshot);
```

- [ ] **Step 5: Run full verification**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build -V
env QT_QPA_PLATFORM=offscreen timeout 2s ./build/src/shatv
env QT_QPA_PLATFORM=offscreen SHATV_SMOKE_MEDIA=/absolute/path/to/local.mp4 ./build/src/shatv --mpv-smoke
```

Expected:

- `ctest` 全绿
- 普通模式在 `timeout` 截断前持续运行
- 如果提供有效媒体，`shatv_mpv_smoke` 与手工 `--mpv-smoke` 都输出 `ShaTV Stage3 mpv smoke ok state=playing`

- [ ] **Step 6: Update TODO and commit**

```bash
git add TODO.md src/application src/player tests
git commit -m "feat: add mpv retry flow"
```

## Self-Review

- Spec coverage:
  - `libmpv` 接入：Task 1, Task 2
  - `QOpenGLWidget + render API`：Task 2
  - 状态回流：Task 3
  - 自动重试：Task 4
- Placeholder scan:
  - 已避免 `TODO/TBD`
  - 所有任务都给出了命令、文件和核心代码片段
- Type consistency:
  - 统一使用 `PlayerBackend` 抽象
  - `MpvRenderWidget` 只在 app/player 层交互
  - `PlayerSnapshot.retry_count` 和 `PlaybackState::kRetrying` 在后续任务中保持一致
  - 真实后端仅在 `Task 2` render 路径接通后接管普通运行路径
