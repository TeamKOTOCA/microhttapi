#!/bin/sh

HOST=localhost
PORT=8001

LOOP=1
PARALLEL=10

PATH_REQ="/test.sh"

# テスト用固定値
NONCE=00000000000000000000000000000
TS=$(date +%s)
HMAC=$(printf "%064s" a | tr ' ' 'a')

REQ="$PATH_REQ nonce=$NONCE ts=$TS hmac=$HMAC"

echo "SEND:"
echo "$REQ"
echo "----- RESPONSE -----"

i=0
while [ $i -lt $LOOP ]; do
  j=0
  while [ $j -lt $PARALLEL ]; do
    {
      printf "%s\n" "$REQ" | nc "$HOST" "$PORT"
    } &
    j=$((j + 1))
  done
  wait
  i=$((i + 1))
done