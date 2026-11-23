### components::music

基于板载无源蜂鸣器异步播放单声部提示音或旋律。新的播放请求会立即替换当前内容，频率为 `0` 的音符表示休止。

```cpp
#include "music/music.h"

static constexpr music::tone_t startup[] {
    { 880, 100 },
    { 0, 50 },
    { 1320, 150 },
};

music::init();
music::play(startup);
```

在 `bsp_hw_init()` 后从任务上下文调用 `music::init()`。音符数据必须使用 `static constexpr` 数组。
暂停会立即静音，恢复时从被中断的音符重新播放。
`play_blocking()` 会等待曲目自然结束；被替换或停止时返回 `false`，不能从 ISR 调用。

调试时，在 `terminal::init(port)` 后调用一次 `music::register_terminal_commands()`，即可从终端测试播放。

```text
music play test
music pause
music resume
music stop
```
