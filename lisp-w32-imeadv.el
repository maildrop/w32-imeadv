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

;; import from font-utils.el
(defun w32-imeadv--import-font-utils--repair-split-list (list-val separator)
  "Repair list LIST-VAL, split at string SEPARATOR, if SEPARATOR was escaped."
  (let ((ret-val nil))
    (while list-val
      (let ((top (pop list-val)))
        (while (string-match-p "\\\\\\'" top)
          (callf concat top separator)
          (when list-val
            (callf concat top (pop list-val))))
        (push top ret-val)))
    (setq ret-val (nreverse ret-val))))
;; import from font-utis.el
(defun w32-imeadv--import-font-utils-name-from-xlfd (xlfd)
  "Return the font-family name from XLFD, a string.

This function accounts for the fact that the XLFD
delimiter, \"-\", is a legal character within fields."
  (let ((elts (w32-imeadv--import-font-utils--repair-split-list
               (split-string
                (replace-regexp-in-string
                 "\\-\\(semi\\|demi\\|half\\|double\\|ultra\\|extra\\)-" "-\\1_" xlfd) "-") "-")))
    (if (>= (length elts) 15)
        (mapconcat 'identity
                   (nreverse
                    (nthcdr
                     12
                     (nreverse
                      (nthcdr 2 elts)))) "-")
      (nth 2 elts))))

(defun w32-imeadv-select-font-family ()
    "IMEが使うフォントをセレクトする"
  (interactive)
  (let ((pos (char-before) ))
    (message (font-utils-name-from-xlfd (face-font 'default nil pos)))
    ))

    
