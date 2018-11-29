(when (load "w32-imeadv")
  (progn 
    (defun w32-imeadv-default-signal-proxy-filter (proc string)
      "process filter"
      (message string) )
    (when (w32-imeadv-initialize)
      (let ((process-connection-type nil ))
	    (start-process "ime" "*ime*"
		               "~/local/bin/emacs-imm32-input-proxy.exe"
		               (number-to-string (w32-imeadv-get-communication-hwnd)))
	    (set-process-filter (get-process "ime") 'w32-imeadv-default-signal-proxy-filter ))
      (w32-imeadv-inject-control (string-to-number (frame-parameter (selected-frame)'window-id))) )))

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
