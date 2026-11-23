##
# @file     CMakeLists.txt
# @brief    Build configuration for Ring 3 command shell (sh) and utilities
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

microkernel_add_user_executable(
    sh
    sh/main.c
    sh/lexer.c
    sh/parser.c
    sh/expand.c
    sh/exec.c
    sh/builtins.c
)
microkernel_add_user_executable(echo utils/echo.c)
microkernel_add_user_executable(cat utils/cat.c)
microkernel_add_user_executable(ls utils/ls.c)
microkernel_add_user_executable(true utils/true.c)
microkernel_add_user_executable(false utils/false.c)
microkernel_add_user_executable(util_test BOOT_NAME test utils/test.c)
microkernel_add_user_executable(mkdir utils/mkdir.c)
microkernel_add_user_executable(rm utils/rm.c)
microkernel_add_user_executable(cp utils/cp.c)
microkernel_add_user_executable(mv utils/mv.c)
microkernel_add_user_executable(uname utils/uname.c)
microkernel_add_user_executable(env utils/env.c)
microkernel_add_user_executable(wc utils/wc.c)
microkernel_add_user_executable(head utils/head.c)
microkernel_add_user_executable(ps utils/ps.c)
microkernel_add_user_executable(getconf utils/getconf.c)
microkernel_add_user_executable(tty utils/tty.c)
microkernel_add_user_executable(stty utils/stty.c)
microkernel_add_user_executable(id utils/id.c)
microkernel_add_user_executable(logname utils/logname.c)
microkernel_add_user_executable(nice utils/nice.c)
microkernel_add_user_executable(renice utils/renice.c)
microkernel_add_user_executable(chmod utils/chmod.c)
microkernel_add_user_executable(chown utils/chown.c)
microkernel_add_user_executable(chgrp utils/chgrp.c)
microkernel_add_user_executable(ln utils/ln.c)
microkernel_add_user_executable(mkfifo utils/mkfifo.c)
microkernel_add_user_executable(readlink utils/readlink.c)
microkernel_add_user_executable(realpath utils/realpath.c)
microkernel_add_user_executable(touch utils/touch.c)
microkernel_add_user_executable(rmdir utils/rmdir.c)
microkernel_add_user_executable(df utils/df.c)
microkernel_add_user_executable(du utils/du.c)
microkernel_add_user_executable(dd utils/dd.c)
microkernel_add_user_executable(basename utils/basename.c)
microkernel_add_user_executable(dirname utils/dirname.c)
microkernel_add_user_executable(pathchk utils/pathchk.c)
microkernel_add_user_executable(sleep utils/sleep.c)
microkernel_add_user_executable(time utils/time.c)
microkernel_add_user_executable(timeout utils/timeout.c)
microkernel_add_user_executable(nohup utils/nohup.c)
microkernel_add_user_executable(locale utils/locale.c)
microkernel_add_user_executable(localedef utils/localedef.c)
microkernel_add_user_executable(iconv utils/iconv.c)
microkernel_add_user_executable(gencat utils/gencat.c)
microkernel_add_user_executable(msgfmt utils/msgfmt.c)
microkernel_add_user_executable(gettext utils/gettext.c)
microkernel_add_user_executable(ngettext utils/ngettext.c)
microkernel_add_user_executable(grep utils/grep.c)
microkernel_add_user_executable(sed utils/sed.c)
microkernel_add_user_executable(find utils/find.c)
microkernel_add_user_executable(xargs utils/xargs.c)
microkernel_add_user_executable(expr utils/expr.c)
microkernel_add_user_executable(printf utils/printf.c)
microkernel_add_user_executable(sort utils/sort.c)
microkernel_add_user_executable(uniq utils/uniq.c)
microkernel_add_user_executable(cut utils/cut.c)
microkernel_add_user_executable(tr utils/tr.c)
microkernel_add_user_executable(tail utils/tail.c)
microkernel_add_user_executable(tee utils/tee.c)
microkernel_add_user_executable(paste utils/paste.c)
microkernel_add_user_executable(join utils/join.c)
microkernel_add_user_executable(cksum utils/cksum.c)
microkernel_add_user_executable(cmp utils/cmp.c)
microkernel_add_user_executable(comm utils/comm.c)
microkernel_add_user_executable(diff utils/diff.c)
microkernel_add_user_executable(patch utils/patch.c)
microkernel_add_user_executable(pax utils/pax.c)
microkernel_add_user_executable(split utils/split.c)
microkernel_add_user_executable(csplit utils/csplit.c)
microkernel_add_user_executable(fold utils/fold.c)
microkernel_add_user_executable(expand utils/expand.c)
microkernel_add_user_executable(unexpand utils/unexpand.c)
microkernel_add_user_executable(strings utils/strings.c)
microkernel_add_user_executable(tsort utils/tsort.c)
microkernel_add_user_executable(file utils/file.c)
microkernel_add_user_executable(pr utils/pr.c)
microkernel_add_user_executable(
    awk
    awk/main.c
    awk/lex.c
    awk/parse.c
    awk/eval.c
)
microkernel_add_user_executable(ed utils/ed.c)
