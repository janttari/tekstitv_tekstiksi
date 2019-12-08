;;; vtx.el --- a package for reading videotext
;; Copyright (C) 1998 Lars Magne Ingebrigtsen

;; Author: Lars Magne Ingebrigtsen <larsi@gnus.org>
;; Keywords: television

;; This file is not part of GNU Emacs, but the same license applies.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;;; Commentary:

;;; Code:

(require 'cl)
(require 'term)

(defvar vtx-tuner-program "set-tv"
  "Command for changing the channel.")

(defvar vtx-channel-file "~/.xawtv"
  "The xawtc startup file.")

(defvar vtx-spool "/tmp/vtx-emacs/"
  "Where the raw vtx files will be written.")

(defvar vtx-grab-program "~/vtx-emacs/vbidecode/vbidecode"
  "Program for grabbing pages.")

(defvar vtx-translate-pages-program "~/vtx-emacs/vtx/vtx2ascii"
  "Program for translating pages into ascii.")

;;; Internal variables

(defvar vtx-channels nil)
(defvar vtx-grab-process nil)
(defvar vtx-current-channel nil)
(defvar vtx-current-page nil)
(defvar vtx-current-version nil)
(defvar vtx-versions nil)
(defvar vtx-play-timer nil)
(defvar vtx-current-pages nil)
(defvar vtx-current-id "")

(defun vtx-init ()
  (unless (file-exists-p vtx-channel-file)
    (error "No xawtv startup file"))
  (with-temp-buffer
    (insert-file-contents vtx-channel-file)
    (setq vtx-channels nil)
    (goto-char (point-min))
    (while (re-search-forward "\\[\\([^]]+\\)\\]" nil t)
      (push (match-string 1) vtx-channels))
    (setq vtx-channels (nreverse vtx-channels))
    (let ((channels vtx-channels)
	  dir)
      (while channels
	(unless (file-exists-p (setq dir (expand-file-name
					  (pop channels) vtx-spool)))
	  (make-directory dir t))))
    (when vtx-grab-process
      (ignore-errors
	(delete-process vtx-grab-process)))
  (vtx-set-channel (car vtx-channels))
  (let ((default-directory vtx-spool))
      (setq vtx-grab-process
	    (start-process "*vtx*" nil vtx-grab-program "-v")))))

(defun vtx-set-channel (channel &optional display)
  (interactive
   (list (completing-read "Channel: " (mapcar 'list vtx-channels) nil t) t))
  (call-process vtx-tuner-program nil nil nil channel)
  (setq vtx-current-channel channel)
  (setq vtx-current-page 100)
  (when display
    (vtx-display-page vtx-current-page))
  (setq vtx-current-pages nil)
  (message "Set channel to %s" channel))

(defun vtx ()
  "Start reading videotext."
  (interactive)
  (vtx-init)
  (pop-to-buffer "*vtx*")
  (term-mode)
  (vtx-mode)
  (vtx-display-page vtx-current-page))

(defvar vtx-mode-map nil)
(unless vtx-mode-map
  (global-set-key [(super j)] 'vtx-visit-current-file)
  
  (setq vtx-mode-map (make-sparse-keymap))
  (suppress-keymap vtx-mode-map)
  (define-key vtx-mode-map "n" 'vtx-next-page)
  (define-key vtx-mode-map "p" 'vtx-prev-page)
  (define-key vtx-mode-map "j" 'vtx-goto-page)
  (define-key vtx-mode-map "s" 'vtx-set-channel)
  (define-key vtx-mode-map "b" 'vtx-prev-version)
  (define-key vtx-mode-map "f" 'vtx-next-version)
  (define-key vtx-mode-map "g" 'vtx-display-page)
  (define-key vtx-mode-map "q" 'vtx-quit)
  )

(defvar vtx-mode nil
  "Mode for vtx buffers.")

(defvar vtx-mode-hook nil
  "Hook run in vtx mode buffers.")

(defun vtx-mode (&optional arg)
  "Mode for vtx buffers.

\\{vtx-mode-map}"
  (interactive (list current-prefix-arg))
  (make-local-variable 'vtx-mode)
  (setq vtx-mode
	(if (null arg) (not vtx-mode)
	  (> (prefix-numeric-value arg) 0)))
  (setq major-mode 'vtx-mode)
  (setq mode-name "vtx")
  (use-local-map vtx-mode-map)
  (setq mode-line-buffer-identification
	'("vtx: " vtx-current-id))
  (setq truncate-lines t)
  (run-hooks 'vtx-mode-hook))

(defun vtx-display-page (page &optional version)
  "Redisplay the current page."
  (interactive (list vtx-current-page))
  (setq vtx-current-page page)
  (erase-buffer)
  (let ((files (vtx-versions page))
	(version (or version (vtx-newest-version page)))
	proc file)
    (setq vtx-versions files)
    (setq vtx-current-version version)
    (setq vtx-current-id
	  (format "%s: %d (%d of %d)" vtx-current-channel
		  vtx-current-page
		  (or version -1) (length files)))
    (if (or
	 (not files)
	 (not (file-exists-p
	       (setq file
		     (expand-file-name
		      (format "%d_%02d.vtx" page (nth (1- version) files))
		      (expand-file-name vtx-current-channel vtx-spool))))))
	(insert "No such file")
      (setq proc
	    (get-buffer-process
	     (term-exec (current-buffer) "*proc*"
			vtx-translate-pages-program nil
			(list "-c" file))))
      (while (memq (process-status proc) '(run open))
	(sit-for 0 50))
      (goto-char (point-max))
      (delete-region (point) (progn (forward-line -1) (point))))
    (goto-char (point-min))
    (set-buffer-modified-p t)
    (set-buffer-modified-p nil)
    (when (and nil (not vtx-play-timer))
      (setq vtx-play-timer (run-at-time 1 5 'vtx-update-page)))))

(defun vtx-quit ()
  "Kill the grab process and bury the buffer"
  (interactive)
  (when vtx-grab-process
    (ignore-errors
      (delete-process vtx-grab-process)))
  (bury-buffer))

(defun vtx-update-page ()
  (ignore-errors
    (when (get-buffer-window (get-buffer "*vtx*"))
      (save-excursion
	(set-buffer "*vtx*")
	(vtx-display-page vtx-current-page)))))

(defun vtx-next-page ()
  "Go to the next page."
  (interactive)
  (unless vtx-current-pages
    (setq vtx-current-pages (vtx-pages)))
  (let ((next (cadr (memq vtx-current-page vtx-current-pages))))
    (unless next
      (error "No next page"))
    (vtx-display-page next)))

(defun vtx-prev-page ()
  "Go to the next page."
  (interactive)
  (unless vtx-current-pages
    (setq vtx-current-pages (vtx-pages)))
  (let ((next (cadr (memq vtx-current-page (reverse vtx-current-pages)))))
    (unless next
      (error "No previous page"))
    (vtx-display-page next)))

(defun vtx-next-version ()
  "Go to the next version."
  (interactive)
  (let ((version (nth vtx-current-version vtx-versions)))
    (unless version
      (error "No next version"))
    (vtx-display-page vtx-current-page (1+ vtx-current-version))))

(defun vtx-prev-version ()
  "Go to the next version."
  (interactive)
  (when (= vtx-current-version 1)
    (error "No previous version"))
  (let ((version (nth (1- vtx-current-version) vtx-versions)))
    (vtx-display-page vtx-current-page (1- vtx-current-version))))

(defun vtx-goto-page (page)
  "Go to a page."
  (interactive
   (list
    (progn
      (unless vtx-current-pages
	(setq vtx-current-pages (vtx-pages)))
      (string-to-number
       (completing-read "Go to page: "
			(mapcar 'list (mapcar 'number-to-string
					      vtx-current-pages))
			nil t)))))
  (vtx-display-page page))

(defun vtx-pages ()
  "Return the list of pages for the current channel."
  (let ((files (directory-files
		(expand-file-name vtx-current-channel vtx-spool)
		nil "\\.vtx$")))
    (delete-duplicates
     (mapcar (lambda (file)
	       (string-to-number (substring file 0 3)))
	     files))))

(defun vtx-versions (page)
  "Return the list of versions of PAGE."
  (let ((files (directory-files
		(expand-file-name vtx-current-channel vtx-spool)
		nil (format "^%d_.*\\.vtx" page))))
    (mapcar (lambda (file)
	      (string-to-number (substring file 5 7)))
	    files)))

(defun vtx-newest-version (page)
  "Return the newest version of PAGE."
  (let ((files (directory-files
		(expand-file-name vtx-current-channel vtx-spool)
		t (format "^%d_.*\\.vtx" page)))
	(i 0))
    (caar
     (sort 
      (mapcar (lambda (file)
		(cons
		 (incf i)
		 (nth 5 (file-attributes file))))
	      files)
      (lambda (f1 f2)
	(time-less-p (cdr f2) (cdr f1)))))))

(defun time-less-p (t1 t2)
  "Say whether time T1 is less than time T2."
  (or (< (car t1) (car t2))
      (and (= (car t1) (car t2))
	   (< (nth 1 t1) (nth 1 t2)))))

(provide 'vtx)

;;; vtx.el ends here
