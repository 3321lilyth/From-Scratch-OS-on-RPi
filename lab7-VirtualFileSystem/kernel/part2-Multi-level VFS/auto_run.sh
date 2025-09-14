#!/bin/bash

MAX_RUN=10000
TIMEOUT=2  # seconds

for ((i=1; i<=MAX_RUN; i++)); do
    echo "=== Running make run: Attempt $i/$MAX_RUN ==="
    
    # 執行 make run 並在背景運行
    timeout $TIMEOUT make run &
    pid=$!

    # 等待 TIMEOUT 秒鐘，如果在 3 秒內結束會立即回傳
    wait $pid
    exit_code=$?

    # timeout 的 exit code 是 124，代表時間到了被 kill 掉（正常情況）
    if [ $exit_code -ne 124 ]; then
        echo ">>> Detected abnormal quick exit at attempt $i (exit code: $exit_code). Stopping."
        break
    fi

    # 確保進程真的沒殘留
    kill -9 $pid 2>/dev/null

    echo "=== Attempt $i done, sleeping 1 second ==="
    sleep 1
done

echo "Script completed."
