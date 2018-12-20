# w32-imeadv (experimental implementation : 実験的実装)

IMM32 dynamic module for Emacs on Windows
これまで、EmacsでIME実用的に使用するためには複雑なパッチを当てる必要があったが、
この作業は煩雑で注意が必要な作業であった。

このパッチ当ての作業を簡素化するために、Dynamic Module を利用したコードを書き下ろすことにした。

# 目標

「できるだけ」GNU 配布のソースを --with-modules でコンパイルしただけで使えるようにすること。
既存の w32-ime.el の機能を満たすこと。互換層を作ること。

- ほとんどのWM_IME_ 周りのコードを、WindowsXP 以降で利用可能になった SetWindowSubclass でサブクラス化することにより行う。
- GNUのコードをできるだけそのままにする。
- Dynamic Module を起点として、 Emacs Lisp の S式を直接呼び出すことはサポートされないので、一旦外部プロセスを経由した、Self-Pipe-Trick を使い、フィルター関数を経由して再度 Dynamic ModuleのS式を呼び出すことにした。

# 注意

まだ、abort の可能性があります。

まだ、デットロックの可能性が排除しきれていない。

- デットロック解除用に 5sec 閾値のタイムアウトを設定しているので、画面がフリーズしたと思ったら、IME制御の状態がおかしくなっているとご理解ください。その場合、できるだけ早くセーブ終了をして作業結果の保全をお願いします。

異字体セレクタがまだ実装できていません。

# 使い方
 Dynamic moduleを使っているので、  --with-modules をつけてコンパイルした Emacs が必要になります。

 本モジュールをビルドして作ったファイル 
- w32-adv.dll
-- これは $(PREFIX)/share/emacs/26.1/site-lisp に配置 これが dynamic module 本体になります

 あとは、追加のサンプルに、lisp-w32-imeadv.el がありますので、どうにかする。

## TODO
- 再変換時にリージョン選択されている時は、そのリージョンを再変換に選ぶようにする。
- 異字体セレクタを考慮すること
- 互換用の w32-ime.el の作成？ 必要？ （これは Lisp に精通する必要があるので遅れる）
- daemon mode で立ち上げた場合に、どうするのかを考える（これは後回し）

## BUG
- そもそも、まだ「動いた」のレベルである。
- まだ、wait message が timeout する場合がある。原因は調査中だが WM_IME* 周りのメッセージが PostMessage で送られてきているのかも知れない。メッセージの待機処理をもう少し練る必要がある。 これは Windows via C/C++ に書いてあったので「正しい」処理に変更した。現在検証中


## できたこと
- WM_IME_ 及びその他のウィンドウメッセージを収奪
- S式へのIMEからの Open/Close 通知
- S式からIMEの Open/Close 制御
- S式へのIMEからのフォント要求通知
- S式からのフォントの設定 (S式へのIMEからのフォント要求通知 からの組み合わせで、現在のface のフォント情報を利用して設定できるようになった)
- 再変換機能ののコード追加
- DocumentFeed 機能のコード追加
- 開いた・閉じたの通知は来るので、mode line をアップデートするように lisp コードを書くこと
- after-make-frame-functions フックを使ってフレーム作成毎に、w32-imeadv-inject-control するようにした。
- 本体のemacs.exe が異常終了したときに、emacs-imm32-input-proxy.exe が残ってしまうので、emacs.exe のプロセスハンドルを開いてMsgWaitForMultipleObjectsで待つように変更すること。
- emacs-imm32-input-proxy.exe を rundll32.exe を使って、dll として導入を図ること。
（そうすれば普通に単一のdllファイルで、w32-imeadv.dll だけで処理できるようになる）

- デバッグログは、マクロNDEBUG が定義されているときは出力されないように変更されました。そして、デフォルトの指示はNDEBUG にする
- UIからデフォルトフォントを変更した後、最初の変換ではフォントの指定が上手くいっていなかった問題に対処した
  この問題は、IMEを開いた状態でデフォルトフォントを変更すると、WM_IME_STARTCOMPOSITIONが送られないのでIMEのフォントを変更するようになっていない問題があった。このために、WM_IME_COMPOSITIONでフォントの指定を指示するようにすると共に、フォントの指定そのものは同期メソッドである必要が無いのでLispスレッドへはPostMessageでメッセージを投げるようにして対応してみた。


## Dynamic Module では実現不可能な内容
- Lispスレッドのコモンダイアログを開くコードが、 UI ウィンドウを親として開くために一時的にデットロックする問題（GNUEmacsBug11732)
- 右下通知領域の IME アイコン の制御は、WM_TIMERのスレッドメッセージ( HWND == nullのメッセージのこと ) が処理しているので、これを修正するためにはメッセージポンプ の スレッドメッセージを Dispatch しない とする部分の修正が必要 （実はメッセージフックで解決可能の道が開かれた？ 要調査）

## 感想
Mircrosoft Windows のフック関数の有能さに救われている。
