[한국어](../../README.md) · [English](README.en.md) · [日本語](README.ja.md) · **简体中文** · [繁體中文](README.zh-TW.md)

# The Missing Floor

这是一款以韩国老式居民楼为背景的第一人称恐怖游戏。你需要寻找失踪哥哥的线索，向邻居打听消息，调查房间和走廊。
到了夜里，敌人会循着声音找过来，就连开门也得小心。

Windows PC · 单人游戏 · 游戏内语言：仅韩语 · 开发中

[下载 Windows 版](https://github.com/easygap/Indie-Game/releases/download/v1.0.0-test.20260922/MissingFloor-Windows-20260922.zip) · [操作说明](#操作说明)

![游戏标题画面：老旧居民楼前显示着韩语主菜单。](../Media/readme/title-menu-first-run-1080.webp)

## 故事背景

为了寻找失联的哥哥，主角搬进了这栋楼的403室。退回的包裹上写着哥哥的地址：501室。可这栋楼明明只有四层。

搬来后的第一晚，凌晨四点半，天花板上传来了敲击声。

![主角的403室，搬家纸箱和生活用品还没收拾好。](../Media/readme/game-bedroom.webp)

## 在附近寻找线索

白天可以拜访邻居，到巷子和便利店打听消息。
居民的只言片语、屋里留下的物品，都可能帮你找到哥哥的下落。

<table>
  <tr>
    <td width="50%"><img src="../Media/readme/game-alley.webp" alt="从居民楼通往便利店的小巷。"></td>
    <td width="50%"><img src="../Media/readme/game-store.webp" alt="便利店的收银台，店员正站在柜台后。"></td>
  </tr>
  <tr>
    <td>楼前的小巷</td>
    <td>附近的便利店</td>
  </tr>
</table>

[查看更多房间和街巷画面 · GIF 4.9MB](../Media/readme/readme-route-preview.gif)

## 入夜后，放轻脚步

在走廊里奔跑，或是猛地打开一扇门，都可能把敌人引过来。
慢慢开门，必要时短暂屏住呼吸，尽量别让对方发现你。

<table>
  <tr>
    <td width="50%"><img src="../Media/readme/game-corridor-day.webp" alt="白天亮着灯的四楼走廊。"></td>
    <td width="50%"><img src="../Media/readme/game-corridor-night.webp" alt="凌晨四点半，同一条走廊陷入昏暗。"></td>
  </tr>
  <tr>
    <td>白天的走廊</td>
    <td>凌晨4:30</td>
  </tr>
</table>

![实机演示：敌人在走廊里靠近并抓住玩家。](../Media/readme/night-listener-chase.gif)

## 调查与解谜

翻看管理室的文件，查看监控和电表箱，也别忘了贴着墙听一听。有些线索藏在墙后的声音里。
白天按 `Tab` 可以回看收集到的记录。卡关时，按 `H` 查看提示。

<table>
  <tr>
    <td width="50%"><img src="../Media/readme/p1-meter-cabinet.webp" alt="电表箱里排列着各房间的电表和抄表记录。"></td>
    <td width="50%"><img src="../Media/readme/game-booth.webp" alt="管理室的桌上放着文件和监控显示器。"></td>
  </tr>
  <tr>
    <td>电表箱</td>
    <td>管理室</td>
  </tr>
</table>

## 下载与运行

目前提供 **2026年9月22日测试版**。**游戏本体暂时仅支持韩语**，菜单、对话和线索文字均为韩语。

1. [下载 Windows 压缩包（ZIP，727MB）](https://github.com/easygap/Indie-Game/releases/download/v1.0.0-test.20260922/MissingFloor-Windows-20260922.zip)。
2. 完整解压后，运行 `IndieGame.exe`。
3. 选择 **게임 시작**（开始游戏）。已有存档时，选择 **이어하기**（继续游戏）。

游戏会自动保存进度。请保留与运行程序放在一起的 `Engine` 和 `IndieGame` 文件夹。

已在 Windows 11、Ryzen 9 7900X、RTX 3060、32GB 内存的电脑上测试。最低配置尚未确定。

如果提示缺少 DLL，请先确认文件已完整解压，再运行 `Engine/Extras/Redist/en-us/vc_redist.x64.exe`。遇到卡顿时，可以尝试调低分辨率或画质。

## 操作说明

使用 `WASD` 移动，鼠标控制视角。按 `E` 查看物品或开门，在门前长按 `E` 可以轻轻开门。

支持手柄，键位和按钮都能在设置中修改。按 `F1` 或手柄方向键上，可重新查看操作说明。

<details>
<summary>完整按键表</summary>

| 按键 | 功能 |
|---|---|
| `W A S D` / 鼠标 | 移动 / 转动视角 |
| `左 Shift` / `C` / `Space` | 奔跑 / 蹲下 / 跳跃 |
| `E` | 查看物品 / 开门 |
| 长按 `E` | 轻轻开门 / 贴墙听声 |
| `Q` / `左 Ctrl` | 敲击 / 屏住呼吸 |
| `F` | 手电筒 |
| `F1` | 目标与操作说明 |
| `Tab` / `H` | 查看记录（白天）/ 提示 |
| `← →` / 鼠标滚轮 | 翻阅文件 |
| `Esc` / `F10` | 暂停 / 无障碍设置 |

</details>

## 难度与辅助设置

不想被追赶的话，可以按 `F10`，选择 **추격 없음**（无追逐）。敌人不会抓住你，解谜和剧情仍可完整体验。另有简单、普通和困难三种难度，游玩途中也能切换。

字幕可以调整字号、背景深浅和显示时间。声音方向可以显示在画面上，敲击声也能通过手柄震动提示。还支持减轻镜头晃动和闪烁、将长按改为单次按键切换，以及分别调整背景音乐和环境音的音量。

![韩语设置菜单，可调整字幕大小、背景深浅和显示时间。](../Media/readme/settings-accessibility-20260922.webp)

游戏包含昏暗场景、突然出现的敌人和音量较大的音效。开始前请先调整音量和亮度。

## 问题与反馈

遇到问题或有想法，可以[在这里留言](https://github.com/easygap/Indie-Game/issues/new)。反馈故障时，请写明发生的场景、之前做了什么，以及具体出现了什么问题。有截图或报错信息也可以一并附上。
