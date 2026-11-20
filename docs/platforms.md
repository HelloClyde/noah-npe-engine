# macOS 与安卓

游戏核心仍为同一套 C11 + SDL2。安卓用 Java 接入系统文件选择和屏幕按键；macOS 用 Cocoa 接入文件选择，不改变 NPE 游戏规则。游戏文件需自行提供。

## macOS

支持 macOS 11 及以上，分别生成 Apple Silicon（arm64）和 Intel（x86_64）版本。

解压后打开 `NoahNPE.app`，选择 NPE 文件。字体和 SDL 均已打包，无需 Homebrew 或另外安装 SDL。
键盘、手柄和三槽存档与 Windows 相同。也可在终端运行：

```sh
./NoahNPE.app/Contents/MacOS/NoahNPE /path/to/game.npe
```

构建需要 Xcode Command Line Tools、CMake、Python 3.12 及以上：

```sh
python3 tools/fetch_sdl.py
cmake -S npe-sdl -B build -DNPE_BUNDLED_SDL=ON -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --parallel
cmake --install build --prefix dist
codesign --force --deep --sign - dist/NoahNPE.app
```

当前使用临时签名，尚未经过 Apple Developer ID 签名和公证，macOS 可能要求用户在系统中确认允许打开。不要关闭系统安全保护。

## 安卓

最低 Android 6.0（API 23），APK 同时含 ARM64 和 x86_64 原生库。应用启动后点“选择并导入 NPE 游戏”，通过系统文件选择器选文件。导入后可离线运行，不申请整个存储空间的读取权限，也不需要联网权限。

- 游戏横屏全画面显示，虚拟键叠加在两侧，不再占用底部按钮条。左侧十字方向盘，右侧采用任天堂方位：X 上、Y 左、A 右、B 下。
- 方向盘支持长按连续移动、滑动换向、中心死区；可同时按方向与动作键。移出按键、切后台或打开弹窗会释放输入，避免持续移动。
- “更多 → 按键透明度”支持 0%～90% 实时预览并保存；数值越大越透明。“更多”入口不随虚拟键变淡，取消会恢复原值。
- “更多 → 按键大小”支持 80%～150%，默认 100%，可实时预览并保存。窄屏和短屏会限制实际放大幅度，避免按键重叠或超出画面。
- “选项左／右”提供原机 J／L，购买数量框可直接切换“是／否”。
- A 确认／对白翻页，B 返回／地图菜单，X 对话／空格翻页，Y 卸装。
- P／N 翻说明；答题页面 A／B／X／Y 分别对应 A／B／C／D。
- 可连接 SDL2 支持的实体手柄，沿用现有手柄映射。
- “更多 → 返回游戏列表”切换游戏；离开前通过游戏菜单保存。
- 游戏副本、字体和存档位于应用私有目录；重复导入同一内容复用同一条目，重名而内容不同的 NPE 不会互相覆盖。
- 存档继续以游戏内容指纹区分。卸载应用会清除导入副本和存档，原始 NPE 不受影响。当前未提供存档导入／导出。

构建需要 Python 3.12+、JDK 17、Gradle 8.9、Android SDK 35、NDK 27.2.12479018、CMake 3.22.1：

```sh
python3 tools/fetch_sdl.py
sdkmanager 'platforms;android-35' 'build-tools;35.0.0' 'ndk;27.2.12479018' 'cmake;3.22.1'
gradle -p android assembleDebug
```

`ANDROID_HOME` 应指向 SDK，或配置 `android/local.properties` 的 `sdk.dir`。输出为 `android/app/build/outputs/apk/debug/app-debug.apk`。CI 提供开发签名 APK，正式分发应使用维护者持有的稳定签名密钥；当前没有上传 Play 商店。

## 构建与验证记录

`.github/workflows/platforms.yml` 构建两个 macOS 包和安卓 APK，上传 Actions 构件；检查包内字体、原生库，并运行 macOS 的 NPE 解析入口。触屏逻辑的方向、地图、对白、剧情、战斗及答题映射有独立 SDL 回归。

这些检查不等于完整真机游玩。真机文件选择、触屏布局、后台恢复和音频仍需分别在目标设备上验证。

参考：[SDL2 安卓移植说明](https://wiki.libsdl.org/SDL2/README-android)、[SDL2 macOS 移植说明](https://wiki.libsdl.org/SDL2/README-macos)、[Android 系统文件选择](https://developer.android.com/training/data-storage/shared/documents-files)。

本次通过的原生构建：[CI 43786777102](https://github.com/HelloClyde/noah-npe-engine/actions/runs/43786777102)（代码 `ff43f20`）。macOS 两种架构均完成包内字体、解析入口和签名校验；安卓两种架构均编译成功并完成包内容检查。Windows 也完成编译、触屏映射与原有行走计时回归。成品见 [v0.2.0](https://github.com/HelloClyde/noah-npe-engine/releases/tag/v0.2.0)。
