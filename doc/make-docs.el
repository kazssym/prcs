;; -*-Mode: Emacs-Lisp;-*-
;; PRCS - The Project Revision Control System
;; Copyright (C) 1997  Josh MacDonald
;;
;; This program is free software; you can redistribute it and/or
;; modify it under the terms of the GNU General Public License
;; as published by the Free Software Foundation; either version 2
;; of the License, or (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program; if not, write to the Free Software
;; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;;
;; $Id: make-docs.el 1.8.1.5 Tue, 20 May 1997 22:48:31 -0700 jmacd $

;; Much of the code in this file comes from the emacs lisp source
;; file makeinfo.el.  It was modified to suit the purpose of
;; generating document strings.  Here is the original copyright:

;;; makeinfo.el --- run makeinfo conveniently

;; Copyright (C) 1991, 1993 Free Software Foundation, Inc.

;; Author: Robert J. Chassell
;; Maintainer: FSF

;; This file is part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to
;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

(defvar makeinfo-run-command "makeinfo"
  "*Command used to run `makeinfo' subjob.
The name of the file is appended to this string, separated by a space.")

(defvar makeinfo-options '("--fill-column=70" "--no-headers" "--no-validate")
  "*String containing options for running `makeinfo'.
Do not include `--footnote-style' or `--paragraph-indent';
the proper way to specify those is with the Texinfo commands
`@footnotestyle` and `@paragraphindent'.")

(require 'texinfo)

(defvar makeinfo-compilation-process nil
  "Process that runs `makeinfo'.  Should start out nil.")

(defvar makeinfo-temp-file nil
  "Temporary file name used for text being sent as input to `makeinfo'.")

(defvar makeinfo-output-file-name nil
  "Info file name used for text output by `makeinfo'.")

;;; The `makeinfo' function definitions

(defun makeinfo-region (region-beginning region-end output-buffer)
  "Make Info file from region of current Texinfo file, and switch to it.

This command does not offer the `next-error' feature since it would
apply to a temporary file, not the original; use the `makeinfo-buffer'
command to gain use of `next-error'."

  (interactive "r")
  (let (filename-or-header
        filename-or-header-beginning
        filename-or-header-end)
    ;; Cannot use `let' for makeinfo-temp-file or
    ;; makeinfo-output-file-name since `makeinfo-compilation-sentinel'
    ;; needs them.

    (setq makeinfo-temp-file
          (concat
           (make-temp-name
            (substring (buffer-file-name)
                       0
                       (or (string-match "\\.tex" (buffer-file-name))
                           (length (buffer-file-name)))))
           ".texinfo"))

    (save-excursion
      (save-restriction
        (widen)
        (goto-char (point-min))
        (let ((search-end (save-excursion (forward-line 100) (point))))
          ;; Find and record the Info filename,
          ;; or else explain that a filename is needed.
          (if (re-search-forward
               "^@setfilename[ \t]+\\([^ \t\n]+\\)[ \t]*"
               search-end t)
              (setq makeinfo-output-file-name
                    (buffer-substring (match-beginning 1) (match-end 1)))
            (error
             "The texinfo file needs a line saying: @setfilename <name>"))

          ;; Find header and specify its beginning and end.
          (goto-char (point-min))
          (if (and
               (prog1
                   (search-forward tex-start-of-header search-end t)
                 (beginning-of-line)
                 ;; Mark beginning of header.
                 (setq filename-or-header-beginning (point)))
               (prog1
                   (search-forward tex-end-of-header nil t)
                 (beginning-of-line)
                 ;; Mark end of header
                 (setq filename-or-header-end (point))))

              ;; Insert the header into the temporary file.
              (write-region
               (min filename-or-header-beginning region-beginning)
               filename-or-header-end
               makeinfo-temp-file nil nil)

            ;; Else no header; insert @filename line into temporary file.
            (goto-char (point-min))
            (search-forward "@setfilename" search-end t)
            (beginning-of-line)
            (setq filename-or-header-beginning (point))
            (forward-line 1)
            (setq filename-or-header-end (point))
            (write-region
             (min filename-or-header-beginning region-beginning)
             filename-or-header-end
             makeinfo-temp-file nil nil))

          ;; Insert the region into the file.
          (write-region
           (max region-beginning filename-or-header-end)
           region-end
           makeinfo-temp-file t nil)

	  (if (not (eq (apply (function call-process)
			     makeinfo-run-command nil output-buffer nil
			     (append makeinfo-options (list makeinfo-temp-file)))
		       0))
	      (error "makeinfo failed--you need makeinfo-3.9.")
	    (if (and makeinfo-temp-file (file-exists-p makeinfo-temp-file))
		(delete-file makeinfo-temp-file))))))))

;; All code below this comment was written by Josh MacDonald and is
;; part of PRCS.

(defun make-new-file(file-name source)
  (let ((buffer (get-buffer-create (generate-new-buffer-name file-name))))
    (set-buffer buffer)

  (insert "/* -*-Mode: C;-*-
 * PRCS - The Project Revision Control System
 * Copyright (C) 1996, 1997  Josh MacDonald
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This file was automatically generated by Emacs and the source
 * file ")
    (insert (expand-file-name source))
    (insert " at ")
    (insert (current-time-string))
    (insert ": DO NOT EDIT.
 */
")
    buffer))

(defun make-m4-file(file-name source)
  (get-buffer-create (generate-new-buffer-name file-name)))

(defun make-docs(file output output-h output-m4)
  "Takes the name of a texinfo file marked with @c BEGIN DOCSTRING name
and @c END DOCSTRING and inserts the region into a c file named output
which contains the region after applying makeinfo"
  (let ((source-buffer (find-file-noselect file))
	(doc-buffer (make-new-file output file))
	(doc-h-buffer (make-new-file output-h file))
	(doc-m4-buffer (make-m4-file output-m4 file)))
    (if (not source-buffer)
	(error "Can't open file %s" file)
      ;; Literal crap at end
      (set-buffer source-buffer)
      (goto-char (point-min))
      (if (re-search-forward "^@c LITERAL")
	  (progn
	    (next-line 1)
	    (beginning-of-line)
	    (let ((beg (point))
		  (end (point-max)))
	      (set-buffer doc-buffer)
	      (insert-buffer-substring source-buffer beg end))))
      ;; Env vars
      (set-buffer source-buffer)
      (goto-char (point-min))
      (let (vars vars-cpy)
	(while (re-search-forward "^@defvr {Environment Variable} " nil t)
	  (let ((beg (match-end 0))
		(end (progn (end-of-line) (point))))
	    (setq vars (cons (buffer-substring-no-properties beg end) vars))))
	(set-buffer doc-h-buffer)
	(insert "\nstruct EnvName { const char* var; const char* defval; };\n")
	(insert "extern const EnvName env_names[];\n")
	(insert "extern const int env_names_count;\n")
	(set-buffer doc-buffer)
	(insert "\n\nconst int env_names_count = ")
	(insert (number-to-string (length vars)))
	(insert ";\n\n")
	(insert "const EnvName env_names[] = {\n")
	(setq vars-cpy vars)
	(while vars-cpy
	  (insert "  { \"" (car vars-cpy) "\", @DEFAULT_ENV_" (car vars-cpy) "@ }")
	  (if (cdr vars-cpy)
	      (insert ","))
	  (insert "\n")
	  (setq vars-cpy (cdr vars-cpy)))
	(insert "};\n")
	(setq vars-cpy vars)
	(set-buffer doc-m4-buffer)
	(insert "AC_DEFUN([PRCS_DEFAULT_ENV_VARS],\n[")
	(while vars-cpy
	  (insert "PRCS_DEFAULT_ENV_VAR([" (car vars-cpy) "])\n")
	  (setq vars-cpy (cdr vars-cpy)))
	(insert "])\n"))
      ;; Doc strings
      (set-buffer source-buffer)
      (goto-char (point-min))
      (while (re-search-forward "^@c BEGIN DOCSTRING \\(\\(\\w\\|[$_]\\)+\\)\n" nil t)
	(let ((source-start (point)))
	  (set-buffer doc-buffer)
	  (insert "\n\const char ")
	  (insert-buffer-substring source-buffer (match-beginning 1) (match-end 1))
	  (insert "[] = \n")
	  (set-buffer doc-h-buffer)
	  (insert "extern const char ")
	  (insert-buffer-substring source-buffer (match-beginning 1) (match-end 1))
	  (insert "[];\n")
	  (set-buffer doc-buffer)
	  (let ((last-doc-beginning (point)))
	    (set-buffer source-buffer)
	    (if (not (search-forward "@c END DOCSTRING"))
		(error "unbalanced docstring markers")
	      (beginning-of-line)
	      (makeinfo-region source-start (point) doc-buffer)
	      (set-buffer doc-buffer)
	      (c-escape-stuff last-doc-beginning (point))
	      (insert ";\n")
	      (set-buffer source-buffer)))))
      ;; Write them
      (set-buffer doc-buffer)
      (write-file output)
      (set-buffer doc-h-buffer)
      (write-file output-h)
      (set-buffer doc-m4-buffer)
      (write-file output-m4)
      (kill-buffer doc-buffer))))

(defun c-escape-stuff(beg end)
    (save-excursion
      (save-restriction
      (narrow-to-region beg end)
      (goto-char (point-min))
      (perform-replace "\"" "\\\"" nil nil nil)
      (goto-char (point-min))
      (while (< (point) (- (point-max) 1))
	(insert "\"")
	(end-of-line)
	(insert "\\n\"")
	(forward-char)))))

(defun make-docs-noargs()
  (setq make-backup-files nil)
  (make-docs "../doc/prcs.texi" "docs.cc.in" "include/docs.h" "../m4/prcsenv.m4"))
