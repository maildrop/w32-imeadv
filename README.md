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

## TODO
- emacs-imm32-input-proxy.exe を rundll32.exe を使って、dll として導入を図ること。
（そうすれば普通に単一のdllファイルで、w32-imeadv.dll だけで処理できるようになる）
- フォントの設定コードの追加
- 再変換機能ののコード追加
- DocumentFeed 機能のコード追加
- 互換用の w32-ime.el の作成 （これは Lisp に精通する必要があるので遅れる）

## できたこと
- WM_IME_ 及びその他のウィンドウメッセージを収奪
- S式でのIMEの Enable/Disable
- S式でのIMEの Open/Close 通知

## Dynamic Module では実現不可能な内容
- Lispスレッドのコモンダイアログを開くコードが、 UI ウィンドウを親として開くために一時的にデットロックする問題（GNUEmacsBug11732)
- 右下通知領域の IME アイコン の制御は、WM_TIMERのスレッドメッセージ( HWND == nullのメッセージのこと ) が処理しているので、これを修正するためにはメッセージポンプ の スレッドメッセージを Dispatch しない とする部分の修正が必要





