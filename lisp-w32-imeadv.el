;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; w32-imeadv 初期化部分
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(when (and (eq system-type 'windows-nt)   ; Windows NT 上で
           window-system                  ; Window システムがあって
           (locate-library "w32-imeadv")) ; w32-imeadvが存在していれば、
  (load "w32-imeadv") ; w32-imeadv をロードする。

  (when (w32-imeadv-initialize) ; w32-imeadv-initialize は失敗することがあります。
                                        ; 通知用のサブプロセス( UIスレッドのイベントを、self-pipe-trick で、入力へ変換する ) の起動
    (let ( (process-connection-type nil )        ; pipe を使います
                                        ;(process-adaptive-read-buffering nil) ; adaprive である必要はありません
           (process-name "emacs-imm32-input-proxy") )
                                        ; サブプロセス(rundll32.exe)の環境を設定するにあたって、PATHの設定をする、
                                        ; exec-pathに記されたディレクトリを走査していって、
                                        ; w32-imeadv.dllが必要とする三つのファイルのうちどれかがあれば、
                                        ; それを環境変数PATHの中に入れておく。

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

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; IME Composition フォントの設定
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (defvar w32-imeadv-composition-font-hook nil)

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
      (interactive)
      ;; フォントの調整をする機会をユーザーに与える
      (let ( (font-attributes
              (if (and (boundp 'w32-imeadv-ime-composition-font-attributes)
                       (not (null w32-imeadv-ime-composition-font-attributes )))
                  w32-imeadv-ime-composition-font-attributes
                (font-face-attributes (face-font 'default nil ?あ )))) ) ; ?あ or (char-before)
        (run-hooks 'w32-imeadv-composition-font-hook)
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
            (setq describe-current-input-method-function 'w32-imeadv-state-switch)
            (setq deactivate-current-input-method-function 'w32-imeadv-state-switch) ))
        (w32-imeadv-on-hook-foreach-buffer-function (cdr list))))

    ;; IME が on になったときに呼ばれるフック関数
    (add-hook 'w32-imeadv-ime-on-hook
              (lambda ()
                (w32-imeadv-on-hook-foreach-buffer-function (buffer-list))
                (unwind-protect (run-hooks 'input-method-activate-hook))))

    (defun w32-imeadv-off-hook-foreach-buffer-function (list)
      "w32-imeadv が off になった時にローカル変数を設定する
current-input-method describe-current-input-method-function deactivate-current-input-method-function
の各変数は、バッファローカルな変数で、それぞれバッファ事に、InputMethodを切り替えることができるようになっているが、WindowsのIMEは、グローバルに作用するので
すべてのバッファの変数をそれぞれ設定しなおす。"
      (when list
        (let ((buffer (car list)))
          (with-current-buffer buffer
            (setq current-input-method nil)
            (setq describe-current-input-method-function nil)
            (setq deactivate-current-input-method-function nil) ))
        (w32-imeadv-off-hook-foreach-buffer-function (cdr list))))

    (defun w32-imeadv-buffer-list-update-hook ()
      "バッファのリストが更新されたときに全てのバッファに対して、IMEの状態を更新するフック
current-input-method describe-current-input-method-function deactivate-current-input-method-function
の各変数は、バッファローカルな変数で、それぞれバッファ事に、InputMethodを切り替えることができるようになっているが、WindowsのIMEは、グローバルに作用するので
すべてのバッファの変数をそれぞれ設定しなおす。"
      (when (string= "W32-IMEADV" default-input-method)
        (let ( (openstatus (w32-imeadv-get-openstatus (string-to-number (frame-parameter (selected-frame) 'window-id))))
	           (w32-imeadv-buffer-list-update-hook-foreach nil) )
          (setq w32-imeadv-buffer-list-update-hook-foreach (lambda (list)
                                                             (when list
                                                               (with-current-buffer (car list)
                                                                 (if openstatus
                                                                     (progn
                                                                       (setq current-input-method "W32-IMEADV")
                                                                       (setq describe-current-input-method-function 'w32-imeadv-state-switch)
                                                                       (setq deactivate-current-input-method-function 'w32-imeadv-state-switch))
                                                                   (progn
                                                                     (setq current-input-method nil)
                                                                     (setq describe-current-input-method-function nil)
                                                                     (setq deactivate-current-input-method-function nil))))
                                                               (funcall w32-imeadv-buffer-list-update-hook-foreach (cdr list)))))
          (funcall w32-imeadv-buffer-list-update-hook-foreach (buffer-list)))))
    (add-hook 'buffer-list-update-hook 'w32-imeadv-buffer-list-update-hook)

    ;; IME が off になったときに呼ばれるフック関数
    (add-hook 'w32-imeadv-ime-off-hook
              (lambda ()
                (unwind-protect
                    (run-hooks 'input-method-deactivate-hook)
                  (w32-imeadv-off-hook-foreach-buffer-function (buffer-list)))))

    )) ;; end of initialize w32-imeadv

;;;;;;;;;;;;;;;;; w32-imeadv の初期化ここまで ;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 追加の設定
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(when (and (eq system-type 'windows-nt)   ; Windows NT 上で
           window-system                  ; Window システムがあって
           (locate-library "w32-imeadv")) ; w32-imeadvが存在していれば、

  ;; ステータスラインの設定
  (defvar w32-imeadv-status-line-format (list "[　]" "[あ]") )
  (defvar w32-imeadv-status-line (nth 0 w32-imeadv-status-line-format) )

  (defun w32-imeadv-status-line-show ()
    "Get a string to be displayed on the mode-line."
    (format "%s" w32-imeadv-status-line ))
  (setq-default mode-line-format (cons '(:eval (w32-imeadv-status-line-show)) mode-line-format))
  ;; IME が on になったときに呼ばれるフック関数
  (add-hook 'w32-imeadv-ime-on-hook
            (lambda ()
              (setq w32-imeadv-status-line (nth 1 w32-imeadv-status-line-format))
              (force-mode-line-update)) )

  ;; IME が off になったときに呼ばれるフック関数
  (add-hook 'w32-imeadv-ime-off-hook
            (lambda ()
              (setq w32-imeadv-status-line (nth 0 w32-imeadv-status-line-format))
              (force-mode-line-update) ) )

  ;; isearch modeに入る時に IME をオフにする
  (add-hook 'isearch-mode-hook 'deactivate-input-method )

  ;; ミニバッファ setup hook で、IME をオフにする
  (add-hook 'minibuffer-setup-hook (lambda ()
                                     (if (minibufferp)
                                         (with-selected-window (minibuffer-selected-window)
                                           (deactivate-input-method) )
                                       (deactivate-input-method))))

  ;; 日本語入力時にカーソルの色を変える設定
  (defvar w32-imeadv-ime-openstatus-indicate-cursor-color-enable nil)
  (when w32-imeadv-ime-openstatus-indicate-cursor-color-enable
    (defvar w32-imeadv-ime-openstatus-indicate-cursor-color "coral4")
    (defvar w32-imeadv-ime-closestatus-indicate-cursor-color (frame-parameter (selected-frame) 'cursor-color))
    (setq w32-imeadv-ime-on-hook-color-stack nil) ; カーソルの色を保持するスタック
    (add-hook 'input-method-activate-hook
              (lambda ()
                (set-cursor-color w32-imeadv-ime-openstatus-indicate-cursor-color)))
    (add-hook 'input-method-deactivate-hook
              (lambda ()
                (set-cursor-color w32-imeadv-ime-closestatus-indicate-cursor-color))))

  ;; 最後にdefault-input-method を W32-IMEADV にする。(これ重要)
  (setq default-input-method "W32-IMEADV"))

