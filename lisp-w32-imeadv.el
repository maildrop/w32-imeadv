;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; w32-imeadv 初期化部分
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(require 'mule)
(defcustom w32-imeadv-ime-composition-font-investigate-char ?あ
  "IMEが使うフォントを探すためのキャラクタ"
  :type 'character
  :group 'mule
  :group 'i18n
  :group 'w32)

(defcustom w32-imeadv-separate-buffer-state nil
  "IMEの有効/無効をバッファ毎に設定する 実験中(experimental)"
  :type 'boolean
  :group 'mule
  :group 'i18n
  :group 'w32)

(defvar default-input-method ) ; mule-cmds.el ; for elint

(when (and (eq system-type 'windows-nt)   ; Windows NT 上で
           window-system                  ; Window システムがあって
           (locate-library "w32-imeadv")) ; w32-imeadvが存在していれば、
  (require 'w32-imeadv) ; w32-imeadv をロードする。

  (when (w32-imeadv-initialize) ; w32-imeadv-initialize は失敗することがあります。
    ;; 通知用のサブプロセス( UIスレッドのイベントを、self-pipe-trick で、入力へ変換する ) の起動
    (let ( (process-connection-type nil ) ; pipe を使います
                                        ;(process-adaptive-read-buffering nil) ; adaprive である必要はありません
           (process-name "emacs-imm32-input-proxy") )
      ;; 以下は w32-imeadv.dll がダイナミックリンク時に必要だった手順 現在はスタティックリンクなので不要
      ;; しかしながら、いつでもダイナミックリンクに戻せるように、十分テストが終わるまでそのままにしておく。
      ;; サブプロセス(rundll32.exe)の環境を設定するにあたって、PATHの設定をする、
      ;; exec-pathに記されたディレクトリを走査していって、
      ;; w32-imeadv.dllが必要とする三つのファイルのうちどれかがあれば、それを環境変数PATHの中に入れておく。
      (let ((process-environment (list (let ((findlibs nil))
                                         (setq findlibs (lambda (list)
                                                          (if list
                                                              (let ((path (car list)))
                                                                (if (or (file-exists-p (concat (file-name-as-directory path) "libstdc++-6.dll"))
                                                                        (file-exists-p (concat (file-name-as-directory path) "libgcc_s_seh-1.dll"))
                                                                        (file-exists-p (concat (file-name-as-directory path) "libwinpthread-1.dll")))
                                                                    (cons path (funcall findlibs (cdr list)))
                                                                  (funcall findlibs (cdr list))))
                                                            '() )))
                                         (concat "PATH=" (mapconcat #'identity (funcall findlibs (cons invocation-directory exec-path)) ";")))))
            (exec-direcotry (file-name-directory (w32-imeadv--get-module-filename))))

        (start-process process-name nil
                       "rundll32.exe"
                       (w32-imeadv--get-module-filename)
                       "EntryPoint"
                       (number-to-string (w32-imeadv--get-communication-hwnd))))
      (set-process-filter (get-process process-name) 'w32-imeadv--defualt-message-input-handler )
      (set-process-query-on-exit-flag (get-process process-name) nil)
      (add-hook 'kill-emacs-hook (lambda () (when (process-live-p (get-process "emacs-imm32-input-proxy"))
                                              (delete-process "emacs-imm32-input-proxy") t) )))

    (defvar w32-imeadv--programmatic-status-change nil
      "内部で、今IMEのon/offが、半角/全角 alt+` 変換 等でおきたのか それとも C-\ のmule-cmd.el 由来のものなのかを判別するために使う

mule-cmd.el 由来の場合 input-method-activate-hook /
input-method-deactivate-hook の呼び出しが mule-cmd.el の内部で行われる
プロセスキーの押下で変換動作が始まる時には、w32-imeadv-ime-{on,off}-hookがこれを模倣するのであるが、mule-cmd.el 経
由で w32-imeadv-state-switchからの呼び出しの場合 w32-imeadv-set-openstatus-open で IME がオン になった時に
UIスレッドからの通知で、 w32-imeadv-ime-{on,off}-hook が起動される。
この場合 二度 input-method-{activate,deactivate}-hookが呼び出されることになり、都合が悪い。
そこで、mule-cmd.el 経由のIMEのon,offの場合 w32-imeadv--programmatic-status-change に t を仕掛けて置き
w32-imeadv-ime-{on,off}-hook では w32-imeadv--programmatic-status-change が t の時は nil を設定
nil の時は、input-method-{activate,deactivate}-hook を呼び出すという動作を行う
")

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; IME Composition フォントの設定
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (defvar w32-imeadv-composition-font-hook nil)

    (defvar w32-imeadv-ime-composition-font-attributes nil
      "IMEが使うフォントを指定する この値がnilの時は、カーソルのある位置のfaceが使用される。nil推奨" )


    ;; w32-imeadv.dll から呼び出される Lisp の関数
    (defun w32-imeadv--notify-composition-font()
      "IMEが使うフォントを選択する。

font-attributes に必要なフォントを設定する。フォントの選定が終わった後、フック関数w32-imeadv-composition-font-hook を呼び出す
この関数は、正確に、(w32-imeadv-advertise-ime-composition-font-internal font-attributes)の戻り値を返さなければならない。

w32-imeadvは、w32-imeadv-advertise-ime-composition-font-internalでUIスレッドに IMEに使用して欲しいフォントを通知するが、
w32-imeadv--notify-composition-font が nil を返すと、UIスレッドの待機を解除するために、呼び出し元のCのソース部分がfallbackでメッセージを送る。
（元々は、関数w32-imeadv--notify-composition-font 自体が何らかの理由で失敗したときにUIを復帰させるためのコード）

このfallback動作が無いと、UIスレッドがフリーズしてしまう。
最悪なのは、UIスレッドがフリーズして動作が復帰しないことなので安全装置としてこの動作はご容赦いただきたい。

そして、w32-imeadv-advertise-ime-composition-font-internal の呼び出しが成功しているにもかかわらずこのfallback動作が発動した場合
通常は無視される動作になるが、別の理由（再変換領域の調査など）でUIスレッドが待機している状態にフォントを通知するタイミングが重なると、
これもまたUIスレッドが待機状態になってタイムアウトを待つ状態になる。（おおよそ3回リトライして、そのインターバルは1秒なので計4秒 大体5秒ぐらい）
よってこの関数は、正確に(w32-imeadv-advertise-ime-composition-font-internal font-attributes)の戻り値を返すことを要求する"
      (let ( (font-attributes
              (if (and (boundp 'w32-imeadv-ime-composition-font-attributes)
                       (not (null w32-imeadv-ime-composition-font-attributes )))
                  w32-imeadv-ime-composition-font-attributes
                ;; get-cahr-property で返される値は、 list の場合がある。 例えば nxml-mode の場合
                (font-face-attributes (face-font (or (let ((property (get-char-property (point) 'face)))
                                                       (if (listp property)
                                                           (car property)
                                                         property ))
                                                     'default)
                                                 nil
                                                 (or w32-imeadv-ime-composition-font-investigate-char ?あ)))))) ; ?あ or (char-before)
        (run-hooks 'w32-imeadv-composition-font-hook) ; フォントの調整をする機会をユーザーに与える
        (w32-imeadv-advertise-ime-composition-font-internal font-attributes )))

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; w32-imeadv のローレベルな有効化
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;; 今は、init.el の中なので、最初のフレームに対して、 w32-imeadv-inject-control する。
    ;; selected-frame（現在のフレーム）にw32-imeadvを導入する
    (w32-imeadv-inject-control (string-to-number (frame-parameter (selected-frame)'window-id)))
    ;; w32-imeadv-inject-control はフレーム毎に行う必要があります。
    (add-hook 'after-make-frame-functions
              (lambda (theframe)
                "w32-imeadv-inject-control"
                (w32-imeadv-inject-control (string-to-number (frame-parameter theframe 'window-id))) ) )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; mule-cmd.el の利用
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;; mule-cmd.el の input-method の仕組みに合わせて、state-switch を作る
    (defun w32-imeadv-state-switch ( &optional arg )
      "w32-imeadv-state-switch method"
      (setq w32-imeadv--programmatic-status-change t)
      (if arg
          (progn
            ;; このw32-imeadv-set-openstatus-open が w32-imeadv-ime-on-hook を呼び
            ;; そこから input-method-activate-hook が呼ばれる
            (w32-imeadv-set-openstatus-open (string-to-number (frame-parameter (selected-frame) 'window-id))))
        (progn
          ;; このw32-imeadv-set-openstatus-close が w32-imeadv-ime-off-hook を呼び
          ;; そこから input-method-deactivate-hook が呼ばれる
          (w32-imeadv-set-openstatus-close (string-to-number (frame-parameter (selected-frame) 'window-id))))))
    ;; mule-cmds.el の input-method に W32-IMEADV を登録する
    (register-input-method "W32-IMEADV" "Japanese" 'w32-imeadv-state-switch "" "Microsoft Windows System IME" )

    (defun w32-imeadv-on-hook-foreach-buffer-function (list)
      "w32-imeadv が on になった時にローカル変数を設定する

current-input-method describe-current-input-method-function deactivate-current-input-method-function
の各変数は、バッファローカルな変数で、それぞれバッファ事に、InputMethodを切り替えることができるようになっているが、WindowsのIMEは、グローバルに作用するので
すべてのバッファの変数をそれぞれ設定しなおす。"
      (when list
        (let ((buffer (car list)))
          (with-current-buffer buffer
            (setq current-input-method "W32-IMEADV")
            (setq current-input-method-title  "W32-IMEADV")
            (setq describe-current-input-method-function 'w32-imeadv-state-switch)
            (setq deactivate-current-input-method-function 'w32-imeadv-state-switch) ))
        (w32-imeadv-on-hook-foreach-buffer-function (cdr list))))

    ;; IME が on になったときに呼ばれるフック関数
    (add-hook 'w32-imeadv-ime-on-hook
              (lambda ()
                (if w32-imeadv-separate-buffer-state
                    (progn (setq current-input-method "W32-IMEADV")
                           (setq current-input-method-title  "W32-IMEADV")
                           (setq describe-current-input-method-function 'w32-imeadv-state-switch)
                           (setq deactivate-current-input-method-function 'w32-imeadv-state-switch))
                  (w32-imeadv-on-hook-foreach-buffer-function (buffer-list)))
                (unless w32-imeadv--programmatic-status-change
                  (unwind-protect (run-hooks 'input-method-activate-hook)))
                (setq w32-imeadv--programmatic-status-change nil)))

    (defun w32-imeadv-off-hook-foreach-buffer-function (list)
      "w32-imeadv が off になった時にローカル変数を設定する

current-input-method describe-current-input-method-function deactivate-current-input-method-function
の各変数は、バッファローカルな変数で、それぞれバッファ事に、InputMethodを切り替えることができるようになっているが、WindowsのIMEは、グローバルに作用するので
すべてのバッファの変数をそれぞれ設定しなおす。"
      (when list
        (let ((buffer (car list)))
          (with-current-buffer buffer
            (setq current-input-method nil)
            (setq current-input-method-title nil)
            (setq describe-current-input-method-function nil)
            (setq deactivate-current-input-method-function nil) ))
        (w32-imeadv-off-hook-foreach-buffer-function (cdr list))))

    ;; IME が off になったときに呼ばれるフック関数
    (add-hook 'w32-imeadv-ime-off-hook
              (lambda ()
                (unless w32-imeadv--programmatic-status-change
                  (run-hooks 'input-method-deactivate-hook))
                (setq w32-imeadv--programmatic-status-change nil)
                (if w32-imeadv-separate-buffer-state
                    (progn (setq current-input-method nil)
                           (setq current-input-method-title nil)
                           (setq describe-current-input-method-function nil)
                           (setq deactivate-current-input-method-function nil) )
                  (w32-imeadv-off-hook-foreach-buffer-function (buffer-list)))))

    (defvar w32-imeadv-buffer--last-buffer nil
      "バッファ毎にIMEの状態を覚えておく方法をとったときの、最後のバッファ")
    (defun w32-imeadv-buffer-switch-hook ()
      "post-command-hookを使って、バッファが切り替わったときに、バッファの状態にIMEの状態を同期させる。"
      (unless (eq (current-buffer)
                  w32-imeadv-buffer--last-buffer)
        (when w32-imeadv-separate-buffer-state
          (if (string= current-input-method "W32-IMEADV")
              (w32-imeadv-set-openstatus-open (string-to-number (frame-parameter (selected-frame) 'window-id)))
            (w32-imeadv-set-openstatus-close (string-to-number (frame-parameter (selected-frame) 'window-id)))))
        (setq w32-imeadv-buffer--last-buffer (current-buffer))))

    (add-hook 'post-command-hook 'w32-imeadv-buffer-switch-hook)

    (defun w32-imeadv-buffer-list-update-hook ()
      "バッファのリストが更新されたときに全てのバッファに対して、IMEの状態を更新するフック

current-input-method describe-current-input-method-function deactivate-current-input-method-function
の各変数は、バッファローカルな変数で、それぞれバッファ事に、InputMethodを切り替えることができるようになっているが、WindowsのIMEは、グローバルに作用するので
すべてのバッファの変数をそれぞれ設定しなおす。"
      (unless w32-imeadv-separate-buffer-state
        (when (string= "W32-IMEADV" default-input-method)
          (let ( (openstatus (w32-imeadv-get-openstatus (string-to-number (frame-parameter (selected-frame) 'window-id))))
	         (w32-imeadv-buffer-list-update-hook-foreach nil) )
            (setq w32-imeadv-buffer-list-update-hook-foreach
                  (lambda (list)
                    (when list
                      (with-current-buffer (car list)
                        (if openstatus
                            (progn
                              (setq current-input-method "W32-IMEADV")
                              (setq current-input-method-title "W32-IMEADV")
                              (setq describe-current-input-method-function 'w32-imeadv-state-switch)
                              (setq deactivate-current-input-method-function 'w32-imeadv-state-switch))
                          (progn
                            (setq current-input-method nil)
                            (setq current-input-method-title nil)
                            (setq describe-current-input-method-function nil)
                            (setq deactivate-current-input-method-function nil))))
                      (funcall w32-imeadv-buffer-list-update-hook-foreach (cdr list)))))
            (funcall w32-imeadv-buffer-list-update-hook-foreach (buffer-list))))))
    (add-hook 'buffer-list-update-hook 'w32-imeadv-buffer-list-update-hook)

    )) ;; end of initialize w32-imeadv

;;;;;;;;;;;;;;;;; w32-imeadv の初期化ここまで ;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 追加の設定
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defcustom w32-imeadv-ime-status-line-indicate-openstatus-enable t
  "IMEの on/off をステータスラインに表示する (非nilの時はステータスラインに表示を行う nilの時には表示をしない) デフォルトは t"
  :type 'boolean
  :group 'mule
  :group 'i18n
  :group 'w32
  :set (lambda (sym val)
         (set-default sym val)
         (force-mode-line-update t) ))

(defcustom w32-imeadv-ime-status-line-indicate-close "-　-"
  "ステータスラインに表示するIMEのon/off表示の off の状態 デフォルトは -　-"
  :type 'string
  :group 'mule
  :group 'i18n
  :group 'w32
  :set (lambda (sym val)
         (set-default sym val)
         (if (string= "W32-IMEADV" current-input-method)
             (setq w32-imeadv-status-line w32-imeadv-ime-status-line-indicate-open)
           (setq w32-imeadv-status-line w32-imeadv-ime-status-line-indicate-close))
         (force-mode-line-update t)))


(defcustom w32-imeadv-ime-status-line-indicate-open "-あ-"
  "ステータスラインに表示するIMEのon/off表示の on の状態 デフォルトは -あ-"
  :type 'string
  :group 'mule
  :group 'i18n
  :group 'w32
  :set (lambda (sym val)
         (set-default sym val)
         (if (string= "W32-IMEADV" current-input-method)
             (setq w32-imeadv-status-line w32-imeadv-ime-status-line-indicate-open)
           (setq w32-imeadv-status-line w32-imeadv-ime-status-line-indicate-close))
         (force-mode-line-update t)))

(defcustom w32-imeadv-ime-openstatus-indicate-cursor-color-enable nil
  "IMEがonの時にカーソルの色を変える (非nilの時は色を変える nilの時は色を変えない）デフォルトは nil"
  :type 'boolean
  :group 'mule
  :group 'i18n
  :group 'w32)

(defcustom w32-imeadv-ime-openstatus-indicate-cursor-color "coral4"
  "IMEがonの時にカーソルの色を変える設定をしているときのIMEがonの時のカーソルの色"
  :type 'color
  :group 'mule
  :group 'i18n
  :group 'w32)
(defvar w32-imeadv-ime-closestatus-indicate-cursor-color nil
  "IMEがoffになった時にフレームのカーソルカラーが戻せなくなった時に戻す色")

(defvar-local w32-imeadv-status-line ""
  "現在のステータスラインに表示するIMEの状態文字列")

(defun w32-imeadv-status-line-show ()
  "Get a string to be displayed on the mode-line."
  (if w32-imeadv-ime-status-line-indicate-openstatus-enable
      (with-current-buffer (current-buffer)
        (format " %s" w32-imeadv-status-line ))
    ""))

;; IMEが有効になっているときに、カーソルのカラーを変える機能
(when (and (eq system-type 'windows-nt)   ; Windows NT 上で
           window-system                  ; Window システムがあって
           (locate-library "w32-imeadv")) ; w32-imeadvが存在していれば、

  ;; emacs の 2022-03-04 のコミット
  ;; bidi.c: L.2924 Emacs fatal error: assertion failed: bidi_it->prev_for_neutral.type != UNKNOWN_BT
  ;; のアサートで落ちるようになった
  ;; 当該の部分のコードを読むと ブラケット文字が来たときそれの直前の文字は描画方向が決まっているという前提条件があって、
  ;; これによると先頭文字がブラケットだとアサートに引っかかるという問題がある。従って w32-imeadv-ime-status-line-indicate-openstatus-{close,open} の
  ;; 初期値を [　] から -　- に変更してブラッケットを使わないようにした。
  (setq-default mode-line-format (cons '(:eval (w32-imeadv-status-line-show)) mode-line-format))

  ;; IME が on になったときに呼ばれるフック関数
  (add-hook 'w32-imeadv-ime-on-hook
            (lambda ()
              (if w32-imeadv-separate-buffer-state
                  (setq w32-imeadv-status-line w32-imeadv-ime-status-line-indicate-open)
                (dolist (the-buffer (buffer-list))
                  (with-current-buffer the-buffer
                    (setq w32-imeadv-status-line w32-imeadv-ime-status-line-indicate-open))))
              (force-mode-line-update t)))

  ;; IME が off になったときに呼ばれるフック関数
  (add-hook 'w32-imeadv-ime-off-hook
            (lambda ()
              (if w32-imeadv-separate-buffer-state
                  (setq w32-imeadv-status-line w32-imeadv-ime-status-line-indicate-close)
                (dolist (the-buffer (buffer-list))
                  (with-current-buffer the-buffer
                    (setq w32-imeadv-status-line w32-imeadv-ime-status-line-indicate-close))))
              (force-mode-line-update t)))

  ;; isearch modeに入る時に IME をオフにする
  (add-hook 'isearch-mode-hook 'deactivate-input-method )

  ;; ミニバッファ setup hook で、IME をオフにする
  (add-hook 'minibuffer-setup-hook
            (lambda ()
              (if (minibufferp) ;; minibuffer なら 直接IMEをoffに変更する元のバッファの current-input-method は、 "W32-IMEADV"のまま
                  (w32-imeadv-set-openstatus-close (string-to-number (frame-parameter (selected-frame) 'window-id)))
                (deactivate-input-method))))

  (add-hook 'minibuffer-exit-hook
            (lambda ()
              (when (minibufferp) ;; 元のバッファの current-input-method は保持されているので、戻す
                (with-current-buffer (window-buffer (minibuffer-selected-window))
                  (if (string= current-input-method "W32-IMEADV")
                      (w32-imeadv-set-openstatus-open (string-to-number (frame-parameter (selected-frame) 'window-id)))
                    (w32-imeadv-set-openstatus-close (string-to-number (frame-parameter (selected-frame) 'window-id))))))))

  (add-hook 'input-method-activate-hook
            (lambda ()
              (when w32-imeadv-ime-openstatus-indicate-cursor-color-enable
                (let ( (color-name w32-imeadv-ime-openstatus-indicate-cursor-color )
                       my-each-frame)
                    (setq my-each-frame (lambda (frames)
                                          (when frames
                                            (let ((mod-list (list (cons 'cursor-color color-name)))
                                                  (theframe (car frames)))
                                              (unless (frame-parameter theframe 'w32-imeadv-cursor-color)
                                                (setq mod-list (append (list (cons 'w32-imeadv-cursor-color (frame-parameter theframe 'cursor-color)))
                                                                       mod-list)))
                                              (modify-frame-parameters (car frames) mod-list))
                                            (funcall my-each-frame (cdr frames)))))
                    (funcall my-each-frame (frame-list))))))

  (add-hook 'input-method-deactivate-hook
            (lambda ()
              (when w32-imeadv-ime-openstatus-indicate-cursor-color-enable
                (let ( my-each-frame )
                  (setq my-each-frame (lambda (frames)
                                        (when frames
                                          (let ((theframe (car frames)))
                                            (if (frame-parameter theframe 'w32-imeadv-cursor-color)
                                                (modify-frame-parameters theframe (list (cons 'cursor-color (frame-parameter theframe 'w32-imeadv-cursor-color))
                                                                                        (cons 'w32-imeadv-cursor-color nil)))
                                              (set-frame-parameter theframe 'cursor-color (or w32-imeadv-ime-closestatus-indicate-cursor-color
                                                                                              (frame-parameter theframe 'foreground-color))))
                                            (funcall my-each-frame (cdr frames))))))
                  (funcall my-each-frame (frame-list)))))))

;; 最後にdefault-input-method を W32-IMEADV にする。(これ重要)
(setq-default default-input-method "W32-IMEADV")

