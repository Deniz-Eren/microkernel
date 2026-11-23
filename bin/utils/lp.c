/**
 * @file     lp.c
 * @brief    Ring 3 POSIX.1-2024 XCU lp print-spooling utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_LIBC_0027 */

#define LP_SPOOL_DIR "/tmp/spool/lp"
#define LP_SEQ_FILE "/tmp/spool/lp/.seq"

static int ensure_lp_dir(const char *dest, char *dest_dir, size_t cap) {
    if (mkdir("/tmp/spool", 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    if (mkdir(LP_SPOOL_DIR, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    (void)snprintf(dest_dir, cap, "%s/%s", LP_SPOOL_DIR, dest);
    if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static unsigned long next_lp_id(void) {
    unsigned long id = 0;
    FILE *fp = fopen(LP_SEQ_FILE, "rb");
    if (fp) {
        (void)fscanf(fp, "%lu", &id);
        (void)fclose(fp);
    }
    id++;
    fp = fopen(LP_SEQ_FILE, "wb");
    if (fp) {
        (void)fprintf(fp, "%lu\n", id);
        (void)fclose(fp);
    }
    return id;
}

static int copy_stream(FILE *in, FILE *out) {
    char buf[512];
    size_t nr = 0;
    while ((nr = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, nr, out) != nr) {
            return -1;
        }
    }
    return ferror(in) ? -1 : 0;
}

int main(int argc, char **argv) {
    const char *dest = getenv("LPDEST");
    if (!dest || dest[0] == '\0') {
        dest = getenv("PRINTER");
    }
    if (!dest || dest[0] == '\0') {
        dest = "default";
    }
    const char *title = "";
    long copies = 1;
    int silent = 0;
    int opt = 0;

    while ((opt = getopt(argc, argv, "cd:mn:o:st:w")) != -1) {
        switch (opt) {
        case 'c':
        case 'm':
        case 'o':
        case 'w':
            break;
        case 'd':
            dest = optarg;
            break;
        case 'n': {
            char *end = NULL;
            copies = strtol(optarg, &end, 10);
            if (!end || *end != '\0' || copies <= 0) {
                (void)fprintf(stderr, "lp: invalid copy count: %s\n", optarg);
                return 1;
            }
            break;
        }
        case 's':
            silent = 1;
            break;
        case 't':
            title = optarg;
            break;
        default:
            (void)fprintf(stderr,
                          "usage: lp [-cmsw] [-d dest] [-n copies] [-o opt] "
                          "[-t title] [file...]\n");
            return 1;
        }
    }

    char dest_dir[256];
    if (ensure_lp_dir(dest, dest_dir, sizeof(dest_dir)) != 0) {
        perror("lp: spool");
        return 1;
    }

    unsigned long id = next_lp_id();
    char data_path[320];
    char meta_path[320];
    (void)snprintf(data_path, sizeof(data_path), "%s/job-%lu.data", dest_dir,
                   id);
    (void)snprintf(meta_path, sizeof(meta_path), "%s/job-%lu.meta", dest_dir,
                   id);

    FILE *out = fopen(data_path, "wb");
    if (!out) {
        perror(data_path);
        return 1;
    }

    int nfiles = 0;
    int rc = 0;
    if (optind >= argc) {
        nfiles = 1;
        if (copy_stream(stdin, out) != 0) {
            rc = 1;
        }
    } else {
        for (int i = optind; i < argc; i++) {
            FILE *in = stdin;
            if (strcmp(argv[i], "-") != 0) {
                in = fopen(argv[i], "rb");
                if (!in) {
                    perror(argv[i]);
                    rc = 1;
                    continue;
                }
            }
            if (copy_stream(in, out) != 0) {
                rc = 1;
            } else {
                nfiles++;
            }
            if (in != stdin) {
                (void)fclose(in);
            }
        }
    }
    (void)fclose(out);

    FILE *mfp = fopen(meta_path, "wb");
    if (mfp) {
        (void)fprintf(mfp, "id=%lu\ndest=%s\ncopies=%ld\ntitle=%s\nfiles=%d\n",
                      id, dest, copies, title, nfiles);
        (void)fclose(mfp);
    }

    if (rc == 0 && !silent) {
        (void)printf("request id is %s-%lu (%d file(s))\n", dest, id, nfiles);
    }
    return rc;
}
