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
