#!/bin/sh

# ---- 設定 ----
KEY_FILE="auth_key.txt"
KEY_LEN=32   # 32バイト = 256bit

# ---- ランダムキー生成 ----
if [ -f "$KEY_FILE" ]; then
    echo "$KEY_FILE already exists."
    exit 1
fi

# /dev/urandom から KEY_LEN バイト生成して hex に変換
KEY=$(dd if=/dev/urandom bs=$KEY_LEN count=1 2>/dev/null | xxd -p)

echo "$KEY" > "$KEY_FILE"
chmod 600 "$KEY_FILE"

echo "AUTH_KEY generated and saved to $KEY_FILE:"
echo "$KEY"
