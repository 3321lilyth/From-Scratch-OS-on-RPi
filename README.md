# 🐧 From-Scratch OS on Raspberry Pi 3

本專案包含 **NYCU OSDI 課程 (OSC2025)** 的 Lab 實作，從零開始在 **Raspberry Pi 3** 上打造作業系統。  
所有 lab 均基於 [課程官方說明](https://nycu-caslab.github.io/OSC2025/) 完成，並搭配 QEMU 以及實體樹莓派驗證。  
參考資料: 
- [Armv8-A manual](https://developer.arm.com/documentation/ddi0487/aa/?lang=en)
- [BCM2836 SoC](https://github.com/Tekki/raspberrypi-documentation/blob/master/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf)
- [Resource](https://s-matyukevich.github.io/raspberry-pi-os/)
---

## 📂 Repository Structure
- lab0-environment/ # 基本環境架設(linker )
- lab1-... # (之後會補上)
- lab2-... # (之後會補上)
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