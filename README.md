# Microhttpi

非力なマシン向けの超小型 HTTP サーバー / API サーバーソフトウェア。  
JSON やシェルスクリプトの出力を返すことに特化しています。

---

## APIサーバーソフトウェア
規定の通信手段は JSON です。HTML や CSS は使用しません。  
`.sh` ファイルを置くと、chunked 送信で標準出力を逐次返すことも可能です。

---

## 初期設定

1. `make` でコンパイルした実行ファイルと同じフォルダーに `www` フォルダーを作成してください。
2. その中に JSON ファイルやシェルスクリプト（`.sh`）を置いてください。

例：

```

./www/index.json
./www/sample.sh

```

### .sh ファイルについて
- クライアントがアクセスすると、まず `[Script started]` のレスポンスを返し、その後スクリプトの出力をチャンク形式で送信します。
- chunked 送信なので、途中で出力が生成されてもクライアントに順次届きます。

---

## 認証機能
現状、簡易認証は未実装です。  
将来的にはタイムスタンプ＋一時コードのハッシュ認証を予定しています。

---

## アクセス例

[http://example.com/index.json](http://example.com/index.json)
[http://example.com/sample.sh](http://example.com/sample.sh)



---

## 動作要件
- Linux 系 OS 推奨
- 規定では TCP ポート 8080 を使用
- 非同期処理（select + non-blocking socket）対応済み