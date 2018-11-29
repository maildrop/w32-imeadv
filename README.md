# w32-imeadv
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

 これまでは、再変換とDocumentFeedの二つの機能は、UI スレッドから、Lispスレッドの変数にアクセスするために（GCと衝突してよく終了していたが）これをS式経由で制御できるようになりそうなので、もう少し安定化と高機能化が図れそうである。

# 注意
まだ、abort の可能性があります。
まだ、デットロックの可能性が排除しきれていない。
- デットロック解除用に 5sec 閾値のタイムアウトを設定しているので、画面がフリーズしたと思ったら、IME制御の状態がおかしくなっているとご理解ください。その場合、できるだけ早くセーブ終了をして作業結果の保全をお願いします。

## TODO
- emacs-imm32-input-proxy.exe を rundll32.exe を使って、dll として導入を図ること。
（そうすれば普通に単一のdllファイルで、w32-imeadv.dll だけで処理できるようになる）
- 再変換機能ののコード追加
- DocumentFeed 機能のコード追加
- 互換用の w32-ime.el の作成 （これは Lisp に精通する必要があるので遅れる）
- daemon mode で立ち上げた場合に、どうするのかを考える（これは後回し）

## できたこと
- WM_IME_ 及びその他のウィンドウメッセージを収奪
- S式へのIMEからの Open/Close 通知
- S式からIMEの Open/Close 制御
- S式へのIMEからのフォント要求通知
- S式からのフォントの設定 (S式へのIMEからのフォント要求通知 からの組み合わせで、現在のface のフォント情報を利用して設定できるようになった)

## Dynamic Module では実現不可能な内容
- Lispスレッドのコモンダイアログを開くコードが、 UI ウィンドウを親として開くために一時的にデットロックする問題（GNUEmacsBug11732)
- 右下通知領域の IME アイコン の制御は、WM_TIMERのスレッドメッセージ( HWND == nullのメッセージのこと ) が処理しているので、これを修正するためにはメッセージポンプ の スレッドメッセージを Dispatch しない とする部分の修正が必要 （実はメッセージフックで解決可能の道が開かれた？ 要調査）

## 感想
Mircrosoft Windows のフック関数の有能さに救われている。




