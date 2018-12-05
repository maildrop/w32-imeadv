(when (load "w32-imeadv")
  (when (w32-imeadv-initialize)
    (let ((process-connection-type nil )
          (process-name "emacs-imm32-input-proxy"))
	  (start-process process-name nil
		             "~/local/bin/emacs-imm32-input-proxy.exe"
		             (number-to-string (w32-imeadv--get-communication-hwnd)))
	  (set-process-filter (get-process process-name) 'w32-imeadv--defualt-message-input-handler )
      (process-kill-without-query (get-process process-name) )

      (add-hook 'kill-emacs-hook (lambda () (delete-process "emacs-imm32-input-proxy") t ))


      )

    ;; 手動オーバーライドするための変数 これを指定しているときには固定値になる
    (defvar w32-imeadv-ime-composition-font-attributes nil)
    ;; w32-imeadv.dll から呼び出される Lisp の関数
    (defun w32-imeadv--notify-composition-font()
      "IMEが使うフォントをセレクトする"
      (interactive)
      (let ( ( w32-imeadv-ime-composition-font-attributes
               (if (and (boundp 'w32-imeadv-ime-composition-font-attributes)
                        (not (null w32-imeadv-ime-composition-font-attributes )))
                   w32-imeadv-ime-composition-font-attributes
                 (font-face-attributes (face-font 'default nil (char-before))))) )
        ;; ここで run-hook 
        (w32-imeadv-advertise-ime-composition-font-internal w32-imeadv-ime-composition-font-attributes )
        ))

    ;; isearch modeに入る時に IME をオフにする
    (defun w32-imeadv-default-isearch-hook()
      "isearch hook"
      (w32-imeadv-set-openstatus-close (string-to-number (frame-parameter (selected-frame) 'window-id))))
    (add-hook 'isearch-mode-hook 'w32-imeadv-default-isearch-hook )

    ;; カレントフレームに w32-imeadv を導入する
    (w32-imeadv-inject-control (string-to-number (frame-parameter (selected-frame)'window-id)))
    
    )) ;; end of initialize w32-imeadv 
