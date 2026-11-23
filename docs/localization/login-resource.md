# v83 登录按钮资源记录

2026-11-23，本机已有 `wz/UI.v83.n6JvQgcI` 备份包含与木质登录牌匹配的 v83 风格按钮。当前兼容的 v154 `UI.nx` 包含另一套登录控件，Save loginID 为浅色背景、Join 为半透明按钮，导致截图中的风格不一致。这不是本轮汉化自绘的新按钮。

本机 `wz/UI.Login.nx` 为指向该备份的相对符号链接。加载器从中读取 `Login.img/Title` 下的按钮和记住账号复选框，以及创建角色前的 `Login.img/RaceSelect` 职业选择页；游戏界面、登录背景、光效与标志仍读原有资源。源文件 SHA-256：

`44a9782de554c0f58c7e5e01dd9af7d2e1295d2fa660a6eaa6c44960e39bfb10`

来源性质：项目既有本机备份，地区来源未重新核实，不标为官方国服资源。新机器需将匹配且来源可用的 v83 UI 资源作为 `UI.Login.nx` 放到数据目录，或建立指向其本机备份的符号链接；缺失时回退到 UI.nx。不要为修登录按钮覆盖整包游戏 UI.nx。该运行资源不包含在 Git 源码中。

职业选择页修复：v154 UI.nx 缺少 RaceSelect，导致只显示森林背景和返回按钮。该页现优先使用同一备份中的完整 v83 三职业资源，缺失时回退 UI.nx；不切换 CustomizeChar（本机 v83 备份不含该节点）。`python3 tests/race-select.py` 使用生产对象离线渲染三职业并点击卡片，截图输出到 `build-macos/race-select-check/`。

2026-11-24：选世界／频道页也优先读取该备份中完整的 `WorldSelect` 节点（木牌、卷轴、底板、频道、选中动画、进入按钮），避免 v83 坐标混用 v154 控件；缺失时仍回退 UI.nx。频道网格采用四列五行，底部预留进入按钮。`python3 tests/world-select.py` 使用生产对象离线绘制 1／20 频道并逐格检查鼠标选中，截图在 `build-macos/world-select-check/`。排查参考了 `references/client/MapleStory-Client/IO/UITypes/UIWorldSelect.cpp`，该版本使用另一套 BtChannel 结构，未移植代码。

2026-11-24：角色选择页优先使用备份的完整 `CharSelect` 节点，修正 v154 资料面板与 v83 卷轴坐标混用；服务器标牌仍保留主包 `Common/selectWorld` 及配套 `selectedWorld` 图层。参考核对 `references/client/MapleStory-Client/IO/UITypes/UICharSelect.cpp`，未移植其另一版本布局。`python3 tests/character-select.py` 使用生产对象离线绘制属性卷轴，截图位于 `build-macos/character-select-check/`。
