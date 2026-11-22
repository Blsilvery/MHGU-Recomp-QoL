# MHGU Recomp QoL

MHGU Recomp 的 Forge 插件，为《怪物猎人 X 世代终极版 Recompiled》提供采集、移动、任务缴纳、道具箱和狩猎便利功能。

English summary: A community QoL plugin for Monster Hunter Generations Ultimate Recompiled.

目前插件版本为 **v1.15.1**。完整的中文说明见 [README_zh-CN.md](README_zh-CN.md)。

## 功能

- 免镐、免虫网采矿和捕虫，单次操作批量采集或剥取
- 加快采集和未被大型怪物发现时的冲刺，并调整冲刺耐力消耗
- 在任务中从道具袋手动缴纳，或自动缴纳主、副任务道具和结算其他精算道具
- 将部分搬运物品放进道具袋，同时保留搬运物品的缴纳规则
- 任务奖励、农场交易车和猫猫探险队奖励全部入箱
- 营地完整道具箱、地图显示、大型怪物标记、狩技栏位、环境温度、艾露猫心情和道具调合选项

菜单开关可在游戏中按 Insert 后进入 MHGU Recomp QoL。插件功能有各自独立开关。

## 目标版本

插件针对 MHGU Recomp 游戏程序 Build ID FB08F1D20FD1204FE7070672777F869B 编译和校验。游戏更新改变此 Build ID 后，插件的地址和指令校验需要重新确认。

## 从源码构建

需要 Windows PowerShell、Python 3 和 Zig 0.13.0。

在项目目录运行：

~~~~powershell
.\build.ps1 -Zig zig -Python python
~~~~

构建出的插件位于 out/personal_gathering.nro。将它复制到 Recomp 模组目录：

~~~~text
mods/MHGU Recomp QoL/romfs/nativeNX/plugins/personal_gathering.nro
~~~~

本仓库不包含游戏本体或提取的游戏资源。初次安装中需要调整菜单版面的功能时，可用 tools/make_arcs.py 从自己的游戏安装中生成相应菜单档案。请勿把游戏文件或生成的游戏资源提交到仓库。

## 来源和授权

本项目保留并修改了 Vuze 的 Field QoL 部分代码，遵循仓库附带的 MIT 授权。原作者和相应源文件署名见 [LICENSE](LICENSE)。其他游戏资源文件不包含在此源码仓库中。
