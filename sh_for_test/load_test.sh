#!/bin/bash
# 並列で軽く負荷をかけるスクリプト
URL="http://localhost:8080/"
COUNT=10000
PARALLEL=1000  # 同時接続数

for i in $(seq 1 $COUNT); do
    {
        curl -s -o /dev/null -w "Request #$i: HTTP %{http_code}\n" "$URL"
    } &

    # 並列数の制御
    if (( i % PARALLEL == 0 )); then
        wait
    fi
done

# 残ったバックグラウンドジョブを待つ
wait
echo "All requests done."