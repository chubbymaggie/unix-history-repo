(File cmutpl.l)
(zap lambda go return tyi memq cond prog)
(valueof lambda tlgetevent caddr)
(transprint lambda go tyi tyo return tyipeek memq cond prog)
(cmu-top-level lambda go tlread tleval tlprint prog)
(tlread lambda return rplacd Cnth list tlquote getdisc eq bcdp memq cxr getd dtpr hunkp stringp numberp or and cons ncons apply assoc go cdr car atom exit %lineread lineread caar add1 princ terpri null setq quote boundp not cond prog)
(tlquote lambda go cdr kwote cddr cadr cons setq car eq reverse return null cond prog)
(tlprint lambda prinlev)
(tlgetevent lambda minus Cnth minusp assoc plusp fixp and car null cond)
(tleval lambda return cons ncons cdar rplacd eval setq prog)
(showevents lambda cdr caddr cddr cond cadr tlprint quote princ terpri car null setq liszt-internal-do mapc for-each)
(matchq1 lambda go cdr setq car null quote equal or return eq cond prog)
(matchq lambda *** explode setq matchq1 atom and cond return prog)
