#!/bin/bash

echo "Gathering server info..."
sleep 1

# ホスト名と日時
echo "Hostname: $(hostname)"
echo "Date: $(date)"
sleep 0.5

# CPU情報
echo "CPU Info:"
lscpu | grep -E 'Model name|CPU\(s\)|Thread'
sleep 0.5

# メモリ使用率
echo "Memory usage:"
free -h
sleep 0.5

# 現在のCPU負荷（1秒ごとに3回）
echo "CPU usage (1s interval):"
for i in $(seq 1 3); do
    # top の batch モードで1回だけ取得
    top -b -n1 | grep "Cpu(s)" | awk '{print "  " $2 + $4 "% used"}'
    sleep 1
done

# ロードアベレージ
echo "Load average:"
uptime | awk -F'load average:' '{print "  " $2}'

# 最後に通知
echo "Server info collection complete!"
