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
           (process-adaptive-read-buffering nil) ; adaprive である必要はありません
           (process-name "emacs-imm32-input-proxy") )
      (start-process process-name nil
                     "rundll32.exe"
                     (w32-imeadv--get-module-filename)
                     "EntryPoint"
                     (number-to-string (w32-imeadv--get-communication-hwnd))) 
      (set-process-filter (get-process process-name) 'w32-imeadv--defualt-message-input-handler )
      (set-process-query-on-exit-flag (get-process process-name) nil)
      (add-hook 'kill-emacs-hook (lambda () (when (process-live-p (get-process "emacs-imm32-input-proxy"))
                                              (delete-process "emacs-imm32-input-proxy") t) )))
    
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; IME Composition フォントの設定
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    
    ;; w32-imeadv.dll から呼び出される Lisp の関数
    (defun w32-imeadv--notify-composition-font()
      "IMEが使うフォントを選択する。"
      (interactive)
      (let ( (w32-imeadv-ime-composition-font-attributes
               (if (and (boundp 'w32-imeadv-ime-composition-font-attributes)
                        (not (null w32-imeadv-ime-composition-font-attributes )))
                   w32-imeadv-ime-composition-font-attributes
                 (font-face-attributes (face-font 'default nil ?あ )))) ) ; ?あ or (char-before)
        ;; TODO 本来はここで run-hook して、フォントの調整をする機会をユーザーに与えるべきである。
        (w32-imeadv-advertise-ime-composition-font-internal w32-imeadv-ime-composition-font-attributes )
        ))

    ;; isearch modeに入る時に IME をオフにする
    (defun w32-imeadv-default-isearch-hook()
      "isearch hook"
      (w32-imeadv-set-openstatus-close (string-to-number (frame-parameter (selected-frame) 'window-id))))
    (add-hook 'isearch-mode-hook 'w32-imeadv-default-isearch-hook )

    ;; ステータスラインの設定 
    (defvar w32-imeadv-status-line-format (list "[　]" "[あ]") )
    (defvar w32-imeadv-status-line (nth 0 w32-imeadv-status-line-format) )
    
    (defun w32-imeadv-status-line-show ()
      "Get a string to be displayed on the mode-line."
      (format "%s" w32-imeadv-status-line ))
    (setq-default mode-line-format (cons '(:eval (w32-imeadv-status-line-show)) mode-line-format))

    ;; IME が off になったときに呼ばれるフック関数
    (add-hook 'w32-imeadv-ime-off-hook
              (lambda ()
                (setq w32-imeadv-status-line (nth 0 w32-imeadv-status-line-format))
                (force-mode-line-update)) )
    ;; IME が on になったときに呼ばれるフック関数
    (add-hook 'w32-imeadv-ime-on-hook
              (lambda ()
                (setq w32-imeadv-status-line (nth 1 w32-imeadv-status-line-format))
                (force-mode-line-update)) )
    
    ;; w32-imeadv-inject-control はフレーム毎に行う必要があります。
    (add-hook 'after-make-frame-functions
              (lambda (theframe)
                "w32-imeadv-inject-control"
                (w32-imeadv-inject-control (string-to-number (frame-parameter theframe 'window-id))) ) )

    ;; 今は、init.el の中なので、最初のフレームに対して、 w32-imeadv-inject-control する。
    ;; selected-frame（現在のフレーム）にw32-imeadvを導入する
    (w32-imeadv-inject-control (string-to-number (frame-parameter (selected-frame)'window-id)))
    (defun w32-imeadv-state-switch ( &optional arg )
      "w32-imeadv-state-switch method"
      (if arg
          (progn
            (setq deactivate-current-input-method-function 'w32-imeadv-state-switch)
            (run-hooks 'input-method-activate-hook)
            (setq describe-current-input-method-function nil)
            (w32-imeadv-set-openstatus-open (string-to-number (frame-parameter (selected-frame) 'window-id))))
        (progn
          (run-hooks 'input-method-deactivate-hook)
          (setq describe-current-input-method-function nil)
          (w32-imeadv-set-openstatus-close (string-to-number (frame-parameter (selected-frame) 'window-id))))))
    ;; mule-cmds.el の input-method に W32-IMEADV を登録する
    (register-input-method "W32-IMEADV" "Japanese" 'w32-imeadv-state-switch "" "W32 System IME" )
    )) ;; end of initialize w32-imeadv

;;;;;;;;;;;;;;;;; w32-imeadv の初期化ここまで ;;;;;;;;;;;;;;;;;;;

;; 追加の設定
(when (and (eq system-type 'windows-nt)   ; Windows NT 上で
           window-system                  ; Window システムがあって
           (locate-library "w32-imeadv")) ; w32-imeadvが存在していれば、
  
  ;; 日本語入力時にカーソルの色を変える設定
  (defvar w32-imeadv-ime-openstatus-indicate-cursor-color-enable nil)
  (when w32-imeadv-ime-openstatus-indicate-cursor-color-enable
    (defvar w32-imeadv-ime-openstatus-indicate-cursor-color "coral4")
    (setq w32-imeadv-ime-on-hook-color-stack nil) ; カーソルの色を保持するスタック
    (add-hook 'w32-imeadv-ime-on-hook
              (lambda ()
                ; フレームのパラメータから、cursor-color を取得して スタックにプッシュ
                (push (frame-parameter (selected-frame) 'cursor-color) w32-imeadv-ime-on-hook-color-stack)
                ; 色を変える
                (set-cursor-color w32-imeadv-ime-openstatus-indicate-cursor-color)))
    (add-hook 'w32-imeadv-ime-off-hook
              (lambda ()
                ; スタックから元のカーソル色を取得して、カーソルの色を変更
                (let ( (cursol-corlor (pop w32-imeadv-ime-on-hook-color-stack)) )
                  (when cursol-corlor
                    (set-cursor-color cursol-corlor ))) )))

  ;; 最後にdefault-input-method を W32-IMEADV にする。(これ重要)
  (setq default-input-method "W32-IMEADV") )
