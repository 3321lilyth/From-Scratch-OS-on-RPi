# 🐧 From-Scratch OS on Raspberry Pi 3

本專案包含 **NYCU OSDI 課程 (OSC2025)** 的 Lab 實作，從零開始在 **Raspberry Pi 3** 上打造作業系統。  
所有 lab 均基於 [課程官方說明](https://nycu-caslab.github.io/OSC2025/) 完成，並搭配 QEMU 以及實體樹莓派驗證。  
參考資料: 
- [Armv8-A manual](https://developer.arm.com/documentation/ddi0487/aa/?lang=en)
- [BCM2836 SoC](https://github.com/Tekki/raspberrypi-documentation/blob/master/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf)
- [Resource](https://s-matyukevich.github.io/raspberry-pi-os/)
---

## Overview
- lab0-基本環境架設
- lab1-Boot, Mini UART / Mailbox
- lab2-Bootloader / Device Tree / Initial Ramdisk / Startup Allocator
...
- lab7-...

---

## 🧪 Lab0 - 基本環境架設
🔗 [Lab0 課程說明文件](https://nycu-caslab.github.io/OSC2025/labs/lab0.html)

### 📖 內容
- 建立開發環境（QEMU + cross compiler + minicom）  
- 撰寫最小化的 Assembly 程式並手動建立 linker script  
- 編譯成 ELF，再轉換成 `kernel8.img`，最後於 QEMU 與樹莓派執行  

### ⚙️ QEMU 執行步驟
```bash
# 建議先開一台 VM，並準備 50GB 以上空間

# 安裝必要工具
sudo apt install gcc-aarch64-linux-gnu qemu-system-aarch64
sudo apt install minicom

# 編譯 Assembly
aarch64-linux-gnu-gcc -c a.S

# 連結 (需要自行撰寫 linker.ld)
aarch64-linux-gnu-ld -T linker.ld -o kernel8.elf a.o

# 轉換成 raw binary (因為 RPi3 bootloader 無法載入 ELF)
aarch64-linux-gnu-objcopy -O binary kernel8.elf kernel8.img

# 使用 QEMU 測試
qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -S -s
```

### 🍓 Raspberry Pi 3 實體執行步驟
```bash
# 假設已經生成 kernel8.img

# 插入 SD 卡並查詢裝置
lsblk   # 確認 SD 卡對應的 /dev/sd*

# 寫入助教提供的 bootable image
sudo dd if=nycuos.img of=/dev/sdb   # 請將 /dev/sdb 換成實際裝置名稱
sync    # 確保寫入完成

# 將 SD 卡插入 Raspberry Pi，並透過 USB 連線
sudo chmod 777 /dev/ttyUSB0    # 根據實際 device 編號調整

# 使用 minicom 與 RPi 溝通
sudo minicom -D /dev/ttyUSB0
```


---

## 🧪 Lab1 - Boot, Mini UART & Mailbox
🔗 [Lab1 課程說明文件](https://nycu-caslab.github.io/OSC2025/labs/lab1.html)

### 📖 內容
- **boot.S**：初始化 `sp`（stack pointer）並清空 `.bss` 段。
- **linker script**：起始位址設為 **`0x80000`** 以符合 RPi3 的 boot 規定，並正確配置 `.text/.rodata/.data/.bss` 與對齊。
- **Mini UART**：設定 **GPIO base address** 與 **ALT (Alternate Function)** 以啟用 mini UART（參考 **BCM2837 手冊 §6.2**）。
- **Mailbox**：CPU 透過 mailbox 向 GPU 發送請求，指定「要詢問的內容」、「回寫的記憶體位址」與「channel」。


### ⚙️ 編譯與 QEMU 執行
```bash
# 產生 kernel8.img
make all

# 在 QEMU 執行
make run

# QEMU + GDB 偵錯
make debug  # 終端機 1：啟動 QEMU（暫停等待 GDB）
make gdb    # 終端機 2：啟動並連線 GDB
# 進入 GDB 後常用指令（可視需求）
layout split     # 分割視窗，同時看反組譯與原始碼/暫存器
si               # 單步執行（step instruction），可觀察 PC 移動
# 其他：break <symbol>、info registers、continue 等
```


---

## 🧪 Lab2 — Bootloader / Device Tree / Initial Ramdisk / Startup Allocator
🔗 [Lab2 課程說明文件](https://nycu-caslab.github.io/OSC2025/labs/lab2.html)

### 📖 內容概要
- 本 Lab 依 **Part** 漸進完成並逐一驗證；我的實作與驗證順序為  
  **part1 → part2 → part5 → part3 → part6 → part4**（**final version = part4**）。
- **Bootloader**：自動將 kernel **load** 到正確位址並跳轉執行，避免每次重編後都要手動燒錄/插拔 SD 卡。
- **Initial Ramdisk (initramfs)**：在尚未掛載真正根檔案系統前，提供一個暫時且小型的檔案系統，方便 kernel 早期階段存取 **drivers / kernel modules**。
- **Device Tree (DT)**：告知 kernel 系統上有哪些周邊裝置、記憶體映射與中斷等硬體描述，協助早期硬體初始化。
- **Startup Memory Allocator**：在正式分配器就緒前提供啟動期所需記憶體，供各子系統初始化其資料結構；同時將不可覆寫的關鍵記憶體區段標記為 **reserved/invalid** 以避免誤用。



---

## 🧪 Lab3 — Exceptions, Timers & Async UART (+ User Program via SVC)
🔗 [Lab3 課程說明文件](https://nycu-caslab.github.io/OSC2025/labs/lab3.html)

### 📖 內容概要
- **Exception Vector Table**  
  當同步/非同步例外發生（如 SVC、IRQ、FIQ、SError），硬體會查表取得 **handler entry** 跳入處理；你會實作對應的 EL1 向量與保存/還原暫存器的入口框架。
- **Timer Handler — One-shot Timer Queue**  
  提供簡單的一次性計時器介面，user 可註冊「幾秒後」執行的任務（例如列印訊息）。中斷到來時從 queue 取出到期事件並喚醒/排入後續處理。
- **UART Handler — 由同步改為非同步**  
  將 busy-wait 的同步 UART 改成 **interrupt-driven（非同步）**：  
  - 避免 kernel 停在輪詢；  
  - 在啟用 timer 的情況下，同步 UART 會被頻繁中斷打斷導致輸入錯亂，改為中斷式可穩定接收並緩衝資料。
- **Part5 — Soft Interrupt Handler Decoupling**  
  原則是 **ISR 要「又短又快」**：在中斷中只做**必要最小工作**（例如把資料從硬體 FIFO 搬到記憶體緩衝，避免溢出），其餘邏輯以 **task queue（deferred work）** 交由下半部慢慢處理，ISR 盡快 return。

### ⚙️ 編譯與 QEMU 執行
- **User program & SVC**  
  助教提供的 `user_program.S` 會以 `svc #0` 觸發 system call。注意必須**先把它組譯/連結成獨立的 `user.img`（raw binary）**，才是真正的 machine code，之後 kernel 以 branch/jump **跳到對應載入位址**執行。為此本 lab 會**額外撰寫一份 user-side Makefile**（與 kernel 的 Makefile 分開）。


## 🧪 Lab4 — Memory Allocator
🔗 [Lab4 課程說明文件](https://nycu-caslab.github.io/OSC2025/labs/lab4.html)
### 📖 內容概要
- 本 Lab 實作三種 allocator：
  - **Startup Allocator**：開機早期使用，供各子系統初始化資料結構的臨時記憶體來源（之後會被正式 allocator 取代）。
  - **Buddy System**（實體頁框分配器）：
    - 以 **2^k 頁**為單位做分配與合併。
    - 兩種實作風格：
      - **FreeList**：每個 order 維護一條空閒串列，合併/分裂時於串列間搬移（時間複雜度 **O(n)** 取決於尋找/移除成本）。
      - **Binary Tree 風格**：用樹或位元標記管理分裂/合併狀態，尋找與合併平均 **O(log N)**。
  - **Dynamic Allocator**（小物件/可變大小配置器，例如 `kmalloc/kfree` 風格）：在 buddy 之上建立，處理細粒度配置與減少外部碎片。
- 本次重點在 **C 語言演算法設計與資料結構**（分割/合併、對齊、碎片處理），與底層裝置/例外處理關聯較小。

---

## 🧪 Lab5 — Threads, Scheduler, Syscalls & Signals
🔗 [Lab5 課程說明文件](https://nycu-caslab.github.io/OSC2025/labs/lab5.html)

### 📖 內容概要
- **Thread 系統**：建立 **user thread** 與 **kernel thread**，提供基本的生命週期管理（建立、排程、終止）。
- **Scheduler**：支援 **cooperative**（thread 自願讓出 CPU）與 **preemptive**（由 **timer** 逾時搶佔）切換。
- **System Call**：實作常見介面：
  - `get_pid()`, `read()`, `write()`, `fork()`, `exec()`, `kill()` …  
  - 使用 `svc #0` 進入 EL1，依 ABI（例如 `x8` 為號碼、`x0–x5` 傳參）分派至對應服務。
- **Signal 機制**：提供 `signal()` 讓 thread 註冊自訂 **signal handler**；他線程送出 signal 時，由 kernel 於合適時機切換至 handler 執行，回覆後返回被中斷的 user 態。

### ⚙️ 編譯與 QEMU 執行
- **互動測試重點**：在外接螢幕**播放影片**（由 `syscall.img` 透過 **mailbox + framebuffer** 顯示）的同時，**shell 仍能流暢回應輸入**（驗證 preemptive 調度與 **非同步 UART**）。  

---

## 🧪 Lab6 — Virtual Memory & Page Tables (MMU / TTBR0 / TTBR1)
🔗 [Lab6 課程說明文件](https://nycu-caslab.github.io/OSC2025/labs/lab6.html)

### 📖 內容概要
- **啟用 MMU 的雙階段流程**
  - **Boot 階段（2-level, identity mapping）**：在 early boot 先建立**簡化的 2-level 1:1 映射**（必需的 code/data 與裝置區域），設定 `MAIR_EL1`/`TCR_EL1`，載入 `TTBRx_EL1`，`DSB; ISB` 後設定 `SCTLR_EL1.M=1` 開啟 MMU，確保系統能安全轉入虛擬位址。
  - **MMU 啟動後（C 程式內切換到 3-level）**：以 C 實作完整的 **3-level page table**，替換 boot 時的暫時表；針對 **Kernel** 與 **User** 建立分離的位址空間。
- **位址空間佈局**
  - **Kernel Upper Half**：將 kernel 映射至**高位元虛擬位址空間（upper half）**，包含 `.text/.rodata/.data/.bss`、直接映射區（如需要）、以及各種 **MMIO（Device-nGnRnE）**。
  - **User Lower Half**：每個行程建立獨立的 **lower half** 映射（程式/資料/stack/heap），切換行程時切換 **`TTBR0_EL1`**。


---

## 🧪 Lab7 — Virtual File System (VFS) & Tmpfs（以 Lab5 為基底）
🔗 [Lab7 課程說明文件](https://nycu-caslab.github.io/OSC2025/labs/lab7.html)

### 📖 內容概要
- **開發基底說明**  
  我在 Lab6 僅完成約 80%（且上板未通過），但 **Lab7 與 MMU 不強相依**，因此本 Lab 直接以 **Lab5 的程式碼為基底** 繼續實作。另外印象中 lab7 也只完成 Basic Exercise 4，後面的進階部分沒有實做
  