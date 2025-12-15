#!/bin/sh

# ---- 設定 ----
KEY="$1"
PATH_="/test.sh"
METHOD="GET"
HOST="localhost:8080"

if [ -z "$KEY" ]; then
    echo "usage: $0 AUTH_KEY"
    exit 1
fi

# ---- nonce / timestamp ----
NONCE=$(dd if=/dev/urandom bs=16 count=1 2>/dev/null | xxd -p)
TS=$(date +%s)

# ---- 署名対象文字列（順序固定） ----
DATA="${NONCE}|${TS}|${METHOD}|${PATH_}"

# ---- HMAC-SHA256 ----
HMAC=$(printf "%s" "$DATA" | \
    openssl dgst -sha256 -hmac "$KEY" | awk '{print $2}')

URL="http://${HOST}${PATH_}?nonce=${NONCE}&ts=${TS}&hmac=${HMAC}"

# ---- リクエスト ----
RESPONSE=$(curl -s "$URL")

# ---- 表示 ----
echo "---- request ----"
echo "URL: $URL"
echo
echo "---- response ----"
echo "$RESPONSE"
