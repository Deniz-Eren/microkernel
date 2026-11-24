##
# @file     openssh.cmake
# @brief    Download and cross-compile OpenSSH Portable for Ring 3 bootfs
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: COMP_IPC_0021

option(MICROKERNEL_BUILD_OPENSSH
    "Download and cross-compile OpenSSH Portable into boot.tar" ON)

if(MICROKERNEL_BUILD_OPENSSH)
    if(NOT MICROKERNEL_BUILD_OPENSSL OR NOT MICROKERNEL_BUILD_ZLIB)
        message(FATAL_ERROR
            "MICROKERNEL_BUILD_OPENSSH requires MICROKERNEL_BUILD_OPENSSL=ON "
            "and MICROKERNEL_BUILD_ZLIB=ON")
    endif()
    include(FetchContent)

    set(MICROKERNEL_OPENSSH_VERSION "9.9p2")
    string(CONCAT MICROKERNEL_OPENSSH_DEFAULT_URL
        "https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/"
        "openssh-${MICROKERNEL_OPENSSH_VERSION}.tar.gz")
    set(MICROKERNEL_OPENSSH_URL
        "${MICROKERNEL_OPENSSH_DEFAULT_URL}"
        CACHE STRING "OpenSSH Portable release archive URL")
    set(MICROKERNEL_OPENSSH_SHA256
        "91aadb603e08cc285eddf965e1199d02585fa94d994d6cae5b41e1721e215673"
        CACHE STRING "OpenSSH Portable release archive SHA-256")

    FetchContent_Declare(
        openssh
        URL "${MICROKERNEL_OPENSSH_URL}"
        URL_HASH "SHA256=${MICROKERNEL_OPENSSH_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(openssh)
    if(NOT openssh_POPULATED)
        FetchContent_Populate(openssh)
    endif()

    set(OPENSSH_SRC "${openssh_SOURCE_DIR}")
    set(OPENSSH_GEN "${CMAKE_BINARY_DIR}/openssh")
    file(MAKE_DIRECTORY "${OPENSSH_GEN}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/openssh_config.h"
        "${OPENSSH_GEN}/config.h"
        COPYONLY
    )

    set(OPENSSH_CFLAGS
        -ffreestanding
        -nostdlib
        -nostdinc
        -fno-builtin
        -fno-stack-protector
        -fno-pic
        -fno-pie
        -fno-asynchronous-unwind-tables
        ${MICROKERNEL_USER_ARCH_CFLAGS}
        -std=gnu17
        -w
        -D_GNU_SOURCE
        -D_DEFAULT_SOURCE
        -DSSHDIR=\"/mnt/etc/ssh\"
        -D_PATH_SSH_PROGRAM=\"/boot/ssh\"
        -D_PATH_SSH_ASKPASS_DEFAULT=\"/boot/ssh-askpass\"
        -D_PATH_SFTP_SERVER=\"/boot/sftp-server\"
        -D_PATH_SSH_KEY_SIGN=\"/boot/ssh-keysign\"
        -D_PATH_SSHD_SESSION=\"/boot/sshd-session\"
        -D_PATH_SSH_PKCS11_HELPER=\"/boot/ssh-pkcs11-helper\"
        -D_PATH_SSH_SK_HELPER=\"/boot/ssh-sk-helper\"
        -D_PATH_SSH_PIDDIR=\"/tmp\"
        -D_PATH_PRIVSEP_CHROOT_DIR=\"/mnt/etc/ssh/empty\"
    )

    set(OPENSSH_INC_DIRS
        "${OPENSSH_GEN}"
        "${OPENSSH_SRC}"
        "${OPENSSH_SRC}/openbsd-compat"
        "${CMAKE_SOURCE_DIR}/cmake/openssh_compat"
        "${SYSROOT_DIR}/usr/include"
        "${CLANG_RESOURCE_DIR}/include"
    )

    set(OPENSSH_COMPAT_SRCS
        openbsd-compat/arc4random.c
        openbsd-compat/arc4random_uniform.c
        openbsd-compat/base64.c
        openbsd-compat/basename.c
        openbsd-compat/bcrypt_pbkdf.c
        openbsd-compat/bindresvport.c
        openbsd-compat/blowfish.c
        openbsd-compat/daemon.c
        openbsd-compat/dirname.c
        openbsd-compat/explicit_bzero.c
        openbsd-compat/fmt_scaled.c
        openbsd-compat/freezero.c
        openbsd-compat/fnmatch.c
        openbsd-compat/getcwd.c
        openbsd-compat/getgrouplist.c
        openbsd-compat/getopt_long.c
        openbsd-compat/glob.c
        openbsd-compat/inet_aton.c
        openbsd-compat/inet_ntoa.c
        openbsd-compat/inet_ntop.c
        openbsd-compat/md5.c
        openbsd-compat/memmem.c
        openbsd-compat/mktemp.c
        openbsd-compat/pwcache.c
        openbsd-compat/readpassphrase.c
        openbsd-compat/reallocarray.c
        openbsd-compat/recallocarray.c
        openbsd-compat/rresvport.c
        openbsd-compat/setenv.c
        openbsd-compat/setproctitle.c
        openbsd-compat/sha1.c
        openbsd-compat/sha2.c
        openbsd-compat/sigact.c
        openbsd-compat/strcasestr.c
        openbsd-compat/strlcat.c
        openbsd-compat/strlcpy.c
        openbsd-compat/strmode.c
        openbsd-compat/strndup.c
        openbsd-compat/strnlen.c
        openbsd-compat/strptime.c
        openbsd-compat/strsep.c
        openbsd-compat/strtoll.c
        openbsd-compat/strtonum.c
        openbsd-compat/strtoull.c
        openbsd-compat/strtoul.c
        openbsd-compat/timingsafe_bcmp.c
        openbsd-compat/vis.c
        openbsd-compat/bsd-asprintf.c
        openbsd-compat/bsd-closefrom.c
        openbsd-compat/bsd-err.c
        openbsd-compat/bsd-flock.c
        openbsd-compat/bsd-getline.c
        openbsd-compat/bsd-getpagesize.c
        openbsd-compat/bsd-getpeereid.c
        openbsd-compat/bsd-malloc.c
        openbsd-compat/bsd-misc.c
        openbsd-compat/bsd-poll.c
        openbsd-compat/bsd-pselect.c
        openbsd-compat/bsd-setres_id.c
        openbsd-compat/bsd-signal.c
        openbsd-compat/bsd-snprintf.c
        openbsd-compat/bsd-statvfs.c
        openbsd-compat/bsd-timegm.c
        openbsd-compat/bsd-waitpid.c
        openbsd-compat/fake-rfc2553.c
        openbsd-compat/kludge-fd_set.c
        openbsd-compat/openssl-compat.c
        openbsd-compat/libressl-api-compat.c
        openbsd-compat/xcrypt.c
        openbsd-compat/port-net.c
    )
    list(TRANSFORM OPENSSH_COMPAT_SRCS PREPEND "${OPENSSH_SRC}/")
    list(APPEND OPENSSH_COMPAT_SRCS
        "${CMAKE_SOURCE_DIR}/cmake/openssh_compat/microkernel_compat.c")

    set(OPENSSH_LIBSSH_SRCS
        ssh_api.c
        ssherr.c
        sshbuf.c
        sshkey.c
        sshbuf-getput-basic.c
        sshbuf-misc.c
        sshbuf-getput-crypto.c
        krl.c
        bitmap.c
        authfd.c
        authfile.c
        canohost.c
        channels.c
        cipher.c
        cipher-aes.c
        cipher-aesctr.c
        cleanup.c
        compat.c
        fatal.c
        hostfile.c
        log.c
        match.c
        moduli.c
        nchan.c
        packet.c
        readpass.c
        ttymodes.c
        xmalloc.c
        addr.c
        addrmatch.c
        atomicio.c
        dispatch.c
        mac.c
        misc.c
        utf8.c
        monitor_fdpass.c
        rijndael.c
        ssh-dss.c
        ssh-ecdsa.c
        ssh-ecdsa-sk.c
        ssh-ed25519-sk.c
        ssh-rsa.c
        dh.c
        msg.c
        progressmeter.c
        dns.c
        entropy.c
        gss-genr.c
        umac.c
        umac128.c
        ssh-pkcs11.c
        smult_curve25519_ref.c
        poly1305.c
        chacha.c
        cipher-chachapoly.c
        cipher-chachapoly-libcrypto.c
        ssh-ed25519.c
        digest-openssl.c
        digest-libc.c
        hmac.c
        ed25519.c
        hash.c
        kex.c
        kex-names.c
        kexdh.c
        kexgex.c
        kexecdh.c
        kexc25519.c
        kexgexc.c
        kexgexs.c
        kexsntrup761x25519.c
        kexmlkem768x25519.c
        sntrup761.c
        kexgen.c
        sftp-realpath.c
        platform-pledge.c
        platform-tracing.c
        platform-misc.c
        sshbuf-io.c
    )
    list(TRANSFORM OPENSSH_LIBSSH_SRCS PREPEND "${OPENSSH_SRC}/")

    set(OPENSSH_SSHD_SRCS
        sshd.c
        platform-listen.c
        servconf.c
        sshpty.c
        srclimit.c
        groupaccess.c
        auth2-methods.c
        dns.c
        fatal.c
        compat.c
        utf8.c
        authfd.c
        canohost.c
        ssh-sk-client.c
    )
    list(TRANSFORM OPENSSH_SSHD_SRCS PREPEND "${OPENSSH_SRC}/")

    set(OPENSSH_SSHD_SESSION_SRCS
        sshd-session.c
        auth-rhosts.c
        auth-passwd.c
        audit.c
        platform.c
        sshpty.c
        sshlogin.c
        servconf.c
        serverloop.c
        auth.c
        auth2.c
        auth2-methods.c
        auth-options.c
        session.c
        auth2-chall.c
        groupaccess.c
        auth-bsdauth.c
        auth2-hostbased.c
        auth2-kbdint.c
        auth2-none.c
        auth2-passwd.c
        auth2-pubkey.c
        auth2-pubkeyfile.c
        monitor.c
        monitor_wrap.c
        auth-krb5.c
        auth2-gss.c
        gss-serv.c
        gss-serv-krb5.c
        loginrec.c
        auth-pam.c
        auth-shadow.c
        auth-sia.c
        sftp-server.c
        sftp-common.c
        sandbox-null.c
        uidswap.c
        ssh-sk-client.c
    )
    list(TRANSFORM OPENSSH_SSHD_SESSION_SRCS PREPEND "${OPENSSH_SRC}/")

    set(OPENSSH_SSH_KEYGEN_SRCS
        ssh-keygen.c
        sshsig.c
        ssh-sk-client.c
    )
    list(TRANSFORM OPENSSH_SSH_KEYGEN_SRCS PREPEND "${OPENSSH_SRC}/")

    add_library(openssh_compat STATIC ${OPENSSH_COMPAT_SRCS})
    target_compile_options(openssh_compat PRIVATE ${OPENSSH_CFLAGS})
    target_include_directories(openssh_compat SYSTEM PRIVATE
        ${OPENSSH_INC_DIRS})
    add_dependencies(openssh_compat
        install_headers install_zlib_headers openssl_sysroot)

    add_library(openssh_libssh STATIC ${OPENSSH_LIBSSH_SRCS})
    target_compile_options(openssh_libssh PRIVATE ${OPENSSH_CFLAGS})
    target_include_directories(openssh_libssh SYSTEM PRIVATE
        ${OPENSSH_INC_DIRS})
    add_dependencies(openssh_libssh
        install_headers install_zlib_headers openssl_sysroot)

    function(microkernel_add_openssh_executable name boot_name)
        add_executable(${name} ${ARGN})
        target_compile_options(${name} PRIVATE ${OPENSSH_CFLAGS})
        target_include_directories(${name} SYSTEM PRIVATE ${OPENSSH_INC_DIRS})
        target_link_options(${name} PRIVATE
            -static
            --no-pie
            -T ${MICROKERNEL_USER_LINKER_SCRIPT}
            -z noseparate-code
            --build-id=none
            ${SYSROOT_DIR}/usr/lib/crt0.o
        )
        set_target_properties(${name} PROPERTIES LINK_DEPENDS
            ${MICROKERNEL_USER_LINKER_SCRIPT})
        target_link_libraries(${name} PRIVATE
            openssh_libssh openssh_compat crypto z c)
        add_dependencies(${name} c z crypto openssh_libssh openssh_compat)
        set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_MODULES
            "${boot_name}=$<TARGET_FILE:${name}>")
        set_property(GLOBAL APPEND PROPERTY MICROKERNEL_BOOT_DEPS ${name})
    endfunction()

    microkernel_add_openssh_executable(openssh_sshd sshd
        ${OPENSSH_SSHD_SRCS})
    microkernel_add_openssh_executable(openssh_sshd_session sshd-session
        ${OPENSSH_SSHD_SESSION_SRCS})
    microkernel_add_openssh_executable(openssh_ssh_keygen ssh-keygen
        ${OPENSSH_SSH_KEYGEN_SRCS})
endif()
