/**
 * @file     tsort.c
 * @brief    Ring 3 tsort topological sort utility targeting POSIX.1-2024 XCU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0023 */

typedef struct {
    char *name;
    size_t *adj;
    size_t adj_len;
    size_t adj_cap;
    size_t indeg;
    int visited;
} node_t;

typedef struct {
    node_t *nodes;
    size_t len;
    size_t cap;
} graph_t;

static void graph_free(graph_t *g) {
    for (size_t i = 0; i < g->len; i++) {
        free(g->nodes[i].name);
        free(g->nodes[i].adj);
    }
    free(g->nodes);
    g->nodes = NULL;
    g->len = 0;
    g->cap = 0;
}

static long graph_find_or_add(graph_t *g, const char *name) {
    for (size_t i = 0; i < g->len; i++) {
        if (strcmp(g->nodes[i].name, name) == 0) {
            return (long)i;
        }
    }
    if (g->len == g->cap) {
        size_t ncap = (g->cap == 0) ? 16 : g->cap * 2;
        node_t *nn = realloc(g->nodes, ncap * sizeof(*nn));
        if (!nn) {
            return -1;
        }
        (void)memset(nn + g->cap, 0, (ncap - g->cap) * sizeof(*nn));
        g->nodes = nn;
        g->cap = ncap;
    }
    char *dup = strdup(name);
    if (!dup) {
        return -1;
    }
    size_t idx = g->len++;
    g->nodes[idx].name = dup;
    return (long)idx;
}

static int graph_add_edge(graph_t *g, size_t u, size_t v) {
    if (u == v) {
        return 0;
    }
    node_t *nu = &g->nodes[u];
    for (size_t i = 0; i < nu->adj_len; i++) {
        if (nu->adj[i] == v) {
            return 0;
        }
    }
    if (nu->adj_len == nu->adj_cap) {
        size_t ncap = (nu->adj_cap == 0) ? 8 : nu->adj_cap * 2;
        size_t *na = realloc(nu->adj, ncap * sizeof(*na));
        if (!na) {
            return -1;
        }
        nu->adj = na;
        nu->adj_cap = ncap;
    }
    nu->adj[nu->adj_len++] = v;
    g->nodes[v].indeg++;
    return 0;
}

static char *read_token(FILE *fp) {
    int ch = 0;
    while ((ch = fgetc(fp)) != EOF && isspace(ch)) {
    }
    if (ch == EOF) {
        return NULL;
    }
    size_t cap = 64;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        return NULL;
    }
    while (ch != EOF && !isspace(ch)) {
        if (len + 2 > cap) {
            size_t ncap = cap * 2;
            char *nb = realloc(buf, ncap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
            cap = ncap;
        }
        buf[len++] = (char)ch;
        ch = fgetc(fp);
    }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    FILE *fp = stdin;
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "--") == 0) {
        argi++;
    }
    if (argc - argi > 1) {
        (void)fprintf(stderr, "usage: tsort [file]\n");
        return 1;
    }
    if (argi < argc && strcmp(argv[argi], "-") != 0) {
        fp = fopen(argv[argi], "rb");
        if (!fp) {
            perror(argv[argi]);
            return 1;
        }
    }

    graph_t g = {0};
    int rc = 0;
    for (;;) {
        char *a = read_token(fp);
        if (!a) {
            break;
        }
        char *b = read_token(fp);
        if (!b) {
            free(a);
            (void)fprintf(stderr, "tsort: odd number of tokens\n");
            rc = 1;
            break;
        }
        long u = graph_find_or_add(&g, a);
        long v = (u >= 0) ? graph_find_or_add(&g, b) : -1;
        free(a);
        free(b);
        if (u < 0 || v < 0 || graph_add_edge(&g, (size_t)u, (size_t)v) != 0) {
            rc = 1;
            break;
        }
    }
    if (fp != stdin) {
        (void)fclose(fp);
    }
    if (rc != 0) {
        graph_free(&g);
        return rc;
    }

    size_t emitted = 0;
    while (emitted < g.len) {
        size_t pick = g.len;
        for (size_t i = 0; i < g.len; i++) {
            if (!g.nodes[i].visited && g.nodes[i].indeg == 0) {
                pick = i;
                break;
            }
        }
        if (pick == g.len) {
            rc = 1;
            (void)fprintf(stderr, "tsort: cycle in data\n");
            for (size_t i = 0; i < g.len; i++) {
                if (!g.nodes[i].visited &&
                    (pick == g.len || g.nodes[i].indeg < g.nodes[pick].indeg)) {
                    pick = i;
                }
            }
        }
        g.nodes[pick].visited = 1;
        (void)puts(g.nodes[pick].name);
        emitted++;
        for (size_t j = 0; j < g.nodes[pick].adj_len; j++) {
            size_t nxt = g.nodes[pick].adj[j];
            if (!g.nodes[nxt].visited && g.nodes[nxt].indeg > 0) {
                g.nodes[nxt].indeg--;
            }
        }
    }

    graph_free(&g);
    return rc;
}
