[한국어](../../README.md) · [English](README.en.md) · [日本語](README.ja.md) · [简体中文](README.zh-CN.md) · **繁體中文**

# The Missing Floor

一款以韓國老舊公寓為背景的第一人稱恐怖遊戲。為了找到失蹤的哥哥，你得向鄰居打聽消息，仔細查看房間和走廊裡的線索。
入夜後，敵人會循著聲音找上門，連開門都得放輕動作。

Windows PC · 單人遊戲 · 遊戲內語言：僅韓文 · 開發中

[下載 Windows 版](https://github.com/easygap/Indie-Game/releases/download/v1.0.0-test.20260922/MissingFloor-Windows-20260922.zip) · [操作說明](#操作說明)

![遊戲標題畫面：老舊公寓前顯示著韓文主選單。](../Media/readme/title-menu-first-run-1080.webp)

## 故事簡介

為了尋找失聯的哥哥，主角搬進了這棟公寓的403室。退回的包裹上寫著哥哥的地址：501室。但這棟公寓只有四層樓。

搬來的第一晚，凌晨四點半，天花板開始傳來敲擊聲。

![主角住的403室，搬家的紙箱和生活用品還沒整理好。](../Media/readme/game-bedroom.webp)

## 到附近走走，打聽消息

白天可以拜訪鄰居，到巷子和便利商店找線索。
住戶說過的話、房裡留下的物品，都可能和哥哥的下落有關。

<table>
  <tr>
    <td width="50%"><img src="../Media/readme/game-alley.webp" alt="公寓前通往便利商店的巷子。"></td>
    <td width="50%"><img src="../Media/readme/game-store.webp" alt="便利商店的櫃檯，店員正在店裡。"></td>
  </tr>
  <tr>
    <td>公寓前的小巷</td>
    <td>附近的便利商店</td>
  </tr>
</table>

[看看房間和附近的街景 · GIF 4.9MB](../Media/readme/readme-route-preview.gif)

## 晚上別弄出太大的聲音

在走廊奔跑，或是用力推開門，都可能把敵人引過來。
慢慢開門，必要時暫時屏住呼吸，盡量別被發現。

<table>
  <tr>
    <td width="50%"><img src="../Media/readme/game-corridor-day.webp" alt="白天亮著燈的四樓走廊。"></td>
    <td width="50%"><img src="../Media/readme/game-corridor-night.webp" alt="凌晨四點半，同一條走廊變得昏暗。"></td>
  </tr>
  <tr>
    <td>白天的走廊</td>
    <td>凌晨4:30</td>
  </tr>
</table>

![實際遊玩畫面：敵人在走廊靠近並抓住玩家。](../Media/readme/night-listener-chase.gif)

## 調查線索，解開謎題

翻閱管理室的文件、查看監視器和電表箱，也可以貼著牆聽聲音。有些線索得靠耳朵才能發現。
白天按 `Tab` 就能回頭查看收集到的紀錄。卡關時，可以按 `H` 看提示。

<table>
  <tr>
    <td width="50%"><img src="../Media/readme/p1-meter-cabinet.webp" alt="電表箱內各房間的電表與抄表紀錄。"></td>
    <td width="50%"><img src="../Media/readme/game-booth.webp" alt="管理室桌上放著文件和監視器螢幕。"></td>
  </tr>
  <tr>
    <td>電表箱</td>
    <td>管理室</td>
  </tr>
</table>

## 下載與執行

目前提供 **2026年9月22日測試版**。**遊戲本體目前僅支援韓文**，選單、對話和線索文字都是韓文。

1. [下載 Windows 壓縮檔（ZIP，727MB）](https://github.com/easygap/Indie-Game/releases/download/v1.0.0-test.20260922/MissingFloor-Windows-20260922.zip)。
2. 完整解壓縮後，執行 `IndieGame.exe`。
3. 選擇 **게임 시작**（開始遊戲）。已有存檔時，選擇 **이어하기**（繼續遊戲）。

遊戲會自動儲存進度。請保留執行檔旁的 `Engine` 和 `IndieGame` 資料夾。

已在 Windows 11、Ryzen 9 7900X、RTX 3060、32GB 記憶體的電腦上測試，最低配備需求尚未確定。

如果出現缺少 DLL 的訊息，請先確認所有檔案都已解壓縮，再執行 `Engine/Extras/Redist/en-us/vc_redist.x64.exe`。若遊戲不順，可以試著降低解析度或畫質。

## 操作說明

用 `WASD` 移動，滑鼠控制視角。按 `E` 查看物品或開門，在門前長按 `E` 可以輕輕開門。

支援遊戲手把，也能在設定中更改按鍵配置。按 `F1` 或手把方向鍵上，即可再次查看操作說明。

<details>
<summary>完整操作表</summary>

| 按鍵 | 動作 |
|---|---|
| `W A S D` / 滑鼠 | 移動 / 轉動視角 |
| `左 Shift` / `C` / `Space` | 奔跑 / 蹲下 / 跳躍 |
| `E` | 查看物品 / 開門 |
| 長按 `E` | 輕輕開門 / 貼牆聽聲音 |
| `Q` / `左 Ctrl` | 敲擊 / 屏住呼吸 |
| `F` | 手電筒 |
| `F1` | 目標與操作說明 |
| `Tab` / `H` | 查看紀錄（白天）/ 提示 |
| `← →` / 滑鼠滾輪 | 翻閱文件 |
| `Esc` / `F10` | 暫停 / 無障礙設定 |

</details>

## 難度與輔助設定

不喜歡被追趕的話，可以按 `F10`，選擇 **추격 없음**（關閉追逐）。敵人不會抓住你，謎題和故事仍能完整玩完。另外也有簡單、普通、困難三種難度，遊戲中隨時都能更改。

字幕的大小、背景深淺和顯示時間都能調整。你可以讓畫面標出聲音的方向，或用手把震動提示敲擊聲。也能減少鏡頭晃動與閃爍、把長按操作改成按一下切換，並分別調整背景音樂和環境音的音量。

![韓文設定選單，可調整字幕大小、背景深淺和顯示時間。](../Media/readme/settings-accessibility-20260922.webp)

遊戲有昏暗場景、突然出現的敵人和較大聲的音效。開始前，請先調整音量和亮度。

## 問題回報

遇到問題或想分享遊玩感想，都可以[在這裡留言](https://github.com/easygap/Indie-Game/issues/new)。回報錯誤時，請附上發生的場景、當時做了什麼，以及遇到的狀況。如果有畫面截圖或錯誤訊息，也可以一起附上。
