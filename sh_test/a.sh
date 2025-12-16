#!/bin/sh

HOST=localhost
PORT=8080

PATH_REQ="/test.sh"
NONCE=00000000000000000000000000000000
TS=$(date +%s)
HMAC=$(printf "%064s" a | tr ' ' 'a')

REQ="$PATH_REQ nonce=$NONCE ts=$TS hmac=$HMAC"

echo "SEND:"
echo "$REQ"
echo

# raw TCP（TLSなし）
printf "%s\n" "$REQ" | openssl s_client -quiet -connect "$HOST:$PORT"
