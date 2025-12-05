#!/bin/bash
# 軽めの負荷テストスクリプト
# localhost:8080 に対して 10 回 GET リクエスト

URL="http://localhost:8080/"
    # -s: curl の進捗を隠す, -w: HTTPステータス表示, -o: 内容も表示
    curl -s -w "\nHTTP Status: %{http_code}\n" "$URL"
    echo "---------------------------"
