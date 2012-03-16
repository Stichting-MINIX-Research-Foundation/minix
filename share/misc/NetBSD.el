;; $NetBSD: NetBSD.el,v 1.6 2009/02/16 21:37:12 uwe Exp $

(defconst netbsd-knf-style
  '(
   ;; (c-auto-newline . nil)
   ;; default indentation level
   (c-basic-offset . 8)
   ;; in which column to add backslashes when macroizing a region
   (c-backslash-column . 78)
   (c-backslash-max-column . 78)
   ;; automatically compact brace-else(if)-brace on one line and
   ;; semi-colon after closing struct brace
   (c-cleanup-list . (brace-else-brace
		      brace-elseif-brace
		      defun-close-semi))
   ;; do not indent lines containing only start-of-comment more than default
   (c-comment-only-line-offset . 0)
   ;; start new lines after braces
   ;; default is: before and after (for all other cases)
   (c-hanging-braces-alist . ((defun-open . (before after))
			      (defun-close . (before after))
			      (block-open . (after))
			      (block-close . c-snug-do-while)
			      (substatement-open . after)
			      (statement-case-open . nil)
			      (brace-list-open . after)
			      (brace-list-close . nil)
			      ))
   ;; where to put newlines around colons
   (c-hanging-colons-alist . (quote ((label after)
				     (case-label after))))
   ;; indent comments syntactically
   (c-indent-comments-syntactically-p . t)
   ;; no spaces needed before a label
   ;; (c-label-minimum-indentation . 0)
   ;; define offsets for some code parts
   (c-offsets-alist . ((arglist-cont-nonempty . 4)
		       (block-open        . 0)
;;		       (block-open        . -)
		       (brace-list-open   . +)
		       (brace-list-intro  . +)
		       (brace-list-entry  . 0)
		       (brace-list-close  . 0)
		       (knr-argdecl       . 0)
		       (knr-argdecl-intro . +)
		       (label             . -)
		       (member-init-intro . ++)
		       (statement-cont    . 4)
		       (substatement-open . 0)
		       (case-label        . 0)))
   ;; XXX: undocumented. Recognize KNR style?
   (c-recognize-knr-p . t)
   ;; indent line when pressing tab, instead of a plain tab character
   (c-tab-always-indent . t)
   ;; use TABs for indentation, not spaces
   (indent-tabs-mode . t)
   ;; set default tab width to 8
   (tab-width . 8)
  )
  "NetBSD KNF Style")

(eval-when-compile (require 'whitespace nil t))

(defcustom netbsd-knf-whitespace-check nil
  "Enable NetBSD KNF whitespace cleanup when saving the buffer.
See also:
  `whitespace-auto-cleanup',
  `whitespace-abort-on-error',
  `whitespace-check-leading-whitespace',
  `whitespace-check-trailing-whitespace',
  `whitespace-check-spacetab-whitespace',
  `whitespace-check-indent-whitespace',
  `whitespace-check-ateol-whitespace'.
NOTES:
1) `whitespace-check-spacetab-whitespace' will replace any RE-match of
   \" +\\t\" with single '\\t' and hence may break tabbing.
2) Both `whitespace-check-spacetab-whitespace' and
   `whitespace-check-indent-whitespace' may alter strings."
  :type 'boolean
  :group 'netbsd-knf)

(defun netbsd-knf-whitespace-cleanup ()
  ;; XXX - emacs 21.4 whitespace.el was badly behaved on blank
  ;; buffers.  This was fixed in 22.1.  I don't know about other
  ;; versions, so these conditions may need to be more restrictive.
  (cond ((> emacs-major-version 21)
	 (whitespace-cleanup-internal))
	(t ;; default
	 (if (save-excursion
	       (goto-char (point-min))
	       (re-search-forward "[^ \t\n]" nil t))
	     (whitespace-cleanup)
	   (delete-region (point-min) (point-max))))))

(defun netbsd-knf-write-contents-hook ()
  (if (and (string-equal c-indentation-style "netbsd knf")
	   (require 'whitespace nil t))
      (if netbsd-knf-whitespace-check
	  (if whitespace-auto-cleanup
	      (netbsd-knf-whitespace-cleanup)
	    (if (and (whitespace-buffer) whitespace-abort-on-error)
		(error (concat "Abort write due to whitespaces in "
			       buffer-file-name)))))
    (remove-hook 'write-contents-hook 'netbsd-knf-write-contents-hook))
  nil)

(defun netbsd-knf-c-mode-hook ()
  ;; Add style and set it for current buffer
  (c-add-style "NetBSD KNF" netbsd-knf-style t)
  ;; useful, but not necessary for the mode
  ;; give syntactic information in message buffer
  ;;(setq c-echo-syntactic-information-p t)
  ;; automatic newlines after special characters
  (setq c-toggle-auto-state 1)
  ;; delete all connected whitespace when pressing delete
  (setq c-toggle-hungry-state 1)
  ;; auto-indent new lines
  (define-key c-mode-base-map "\C-m" 'newline-and-indent)
  ;; check/cleanup whitespace when saving
  (add-hook 'write-contents-hooks 'netbsd-knf-write-contents-hook))

(add-hook 'c-mode-hook 'netbsd-knf-c-mode-hook)
