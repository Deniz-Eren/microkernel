##
# @file     CMakeLists.txt
# @brief    Build configuration for Ring 3 command shell (sh) and utilities
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# All bin/ executables link as ET_DYN PIE executables with PT_INTERP=/boot/ld.so
# and DT_NEEDED=libc.so through the user-dyn.ld script; /boot/ld.so resolves
# libc.so from boot.tar at load time (ADR_0059). The primary Ring 3 boot servers
# in servers/ stay statically linked.

microkernel_add_user_dynamic_executable(
    sh
    sh/main.c
    sh/lexer.c
    sh/parser.c
    sh/expand.c
    sh/exec.c
    sh/builtins.c
)
microkernel_add_user_dynamic_executable(echo utils/echo.c)
microkernel_add_user_dynamic_executable(cat utils/cat.c)
microkernel_add_user_dynamic_executable(ls utils/ls.c)
microkernel_add_user_dynamic_executable(true utils/true.c)
microkernel_add_user_dynamic_executable(false utils/false.c)
microkernel_add_user_dynamic_executable(util_test BOOT_NAME test utils/test.c)
microkernel_add_user_dynamic_executable(mkdir utils/mkdir.c)
microkernel_add_user_dynamic_executable(rm utils/rm.c)
microkernel_add_user_dynamic_executable(cp utils/cp.c)
microkernel_add_user_dynamic_executable(mv utils/mv.c)
microkernel_add_user_dynamic_executable(uname utils/uname.c)
microkernel_add_user_dynamic_executable(env utils/env.c)
microkernel_add_user_dynamic_executable(wc utils/wc.c)
microkernel_add_user_dynamic_executable(head utils/head.c)
microkernel_add_user_dynamic_executable(ps utils/ps.c)
microkernel_add_user_dynamic_executable(getconf utils/getconf.c)
microkernel_add_user_dynamic_executable(tty utils/tty.c)
microkernel_add_user_dynamic_executable(stty utils/stty.c)
microkernel_add_user_dynamic_executable(id utils/id.c)
microkernel_add_user_dynamic_executable(logname utils/logname.c)
microkernel_add_user_dynamic_executable(nice utils/nice.c)
microkernel_add_user_dynamic_executable(renice utils/renice.c)
microkernel_add_user_dynamic_executable(chmod utils/chmod.c)
microkernel_add_user_dynamic_executable(chown utils/chown.c)
microkernel_add_user_dynamic_executable(chgrp utils/chgrp.c)
microkernel_add_user_dynamic_executable(ln utils/ln.c)
microkernel_add_user_dynamic_executable(mkfifo utils/mkfifo.c)
microkernel_add_user_dynamic_executable(readlink utils/readlink.c)
microkernel_add_user_dynamic_executable(realpath utils/realpath.c)
microkernel_add_user_dynamic_executable(touch utils/touch.c)
microkernel_add_user_dynamic_executable(rmdir utils/rmdir.c)
microkernel_add_user_dynamic_executable(df utils/df.c)
microkernel_add_user_dynamic_executable(du utils/du.c)
microkernel_add_user_dynamic_executable(dd utils/dd.c)
microkernel_add_user_dynamic_executable(basename utils/basename.c)
microkernel_add_user_dynamic_executable(dirname utils/dirname.c)
microkernel_add_user_dynamic_executable(pathchk utils/pathchk.c)
microkernel_add_user_dynamic_executable(sleep utils/sleep.c)
microkernel_add_user_dynamic_executable(time utils/time.c)
microkernel_add_user_dynamic_executable(timeout utils/timeout.c)
microkernel_add_user_dynamic_executable(nohup utils/nohup.c)
microkernel_add_user_dynamic_executable(locale utils/locale.c)
microkernel_add_user_dynamic_executable(localedef utils/localedef.c)
microkernel_add_user_dynamic_executable(iconv utils/iconv.c)
microkernel_add_user_dynamic_executable(gencat utils/gencat.c)
microkernel_add_user_dynamic_executable(msgfmt utils/msgfmt.c)
microkernel_add_user_dynamic_executable(gettext utils/gettext.c)
microkernel_add_user_dynamic_executable(ngettext utils/ngettext.c)
microkernel_add_user_dynamic_executable(grep utils/grep.c)
microkernel_add_user_dynamic_executable(sed utils/sed.c)
microkernel_add_user_dynamic_executable(find utils/find.c)
microkernel_add_user_dynamic_executable(xargs utils/xargs.c)
microkernel_add_user_dynamic_executable(expr utils/expr.c)
microkernel_add_user_dynamic_executable(printf utils/printf.c)
microkernel_add_user_dynamic_executable(sort utils/sort.c)
microkernel_add_user_dynamic_executable(uniq utils/uniq.c)
microkernel_add_user_dynamic_executable(cut utils/cut.c)
microkernel_add_user_dynamic_executable(tr utils/tr.c)
microkernel_add_user_dynamic_executable(tail utils/tail.c)
microkernel_add_user_dynamic_executable(tee utils/tee.c)
microkernel_add_user_dynamic_executable(paste utils/paste.c)
microkernel_add_user_dynamic_executable(join utils/join.c)
microkernel_add_user_dynamic_executable(cksum utils/cksum.c)
microkernel_add_user_dynamic_executable(cmp utils/cmp.c)
microkernel_add_user_dynamic_executable(comm utils/comm.c)
microkernel_add_user_dynamic_executable(diff utils/diff.c)
microkernel_add_user_dynamic_executable(patch utils/patch.c)
microkernel_add_user_dynamic_executable(pax utils/pax.c)
microkernel_add_user_dynamic_executable(split utils/split.c)
microkernel_add_user_dynamic_executable(csplit utils/csplit.c)
microkernel_add_user_dynamic_executable(fold utils/fold.c)
microkernel_add_user_dynamic_executable(expand utils/expand.c)
microkernel_add_user_dynamic_executable(unexpand utils/unexpand.c)
microkernel_add_user_dynamic_executable(strings utils/strings.c)
microkernel_add_user_dynamic_executable(tsort utils/tsort.c)
microkernel_add_user_dynamic_executable(file utils/file.c)
microkernel_add_user_dynamic_executable(pr utils/pr.c)
microkernel_add_user_dynamic_executable(
    awk
    awk/main.c
    awk/lex.c
    awk/parse.c
    awk/eval.c
)
microkernel_add_user_dynamic_executable(ed utils/ed.c)
microkernel_add_user_dynamic_executable(
    bc
    bc/main.c
    bc/num.c
    bc/eval.c
)
microkernel_add_user_dynamic_executable(m4 utils/m4.c)
microkernel_add_user_dynamic_executable(ldconfig utils/ldconfig.c)
microkernel_add_user_dynamic_executable(uuencode utils/uuencode.c)
microkernel_add_user_dynamic_executable(uudecode utils/uudecode.c)
microkernel_add_user_dynamic_executable(logger utils/logger.c)
microkernel_add_user_dynamic_executable(tput utils/tput.c)
microkernel_add_user_dynamic_executable(tabs utils/tabs.c)
microkernel_add_user_dynamic_executable(mesg utils/mesg.c)
microkernel_add_user_dynamic_executable(write utils/write.c)
microkernel_add_user_dynamic_executable(newgrp utils/newgrp.c)
microkernel_add_user_dynamic_executable(at utils/at.c)
microkernel_add_user_dynamic_executable(batch utils/batch.c)
microkernel_add_user_dynamic_executable(crontab utils/crontab.c)
microkernel_add_user_dynamic_executable(lp utils/lp.c)
microkernel_add_user_dynamic_executable(mailx utils/mailx.c)
microkernel_add_user_dynamic_executable(nc utils/nc.c)
microkernel_add_user_dynamic_executable(ping utils/ping.c)
microkernel_add_user_dynamic_executable(
    sshd_setup BOOT_NAME sshd-setup utils/sshd_setup.c
)
