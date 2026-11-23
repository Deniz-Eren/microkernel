/**
 * @file     wctype.c
 * @brief    POSIX.1-2024 wide character classification and case conversion
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#include <string.h>
#include <wctype.h>
#include "locale_internal.h"

/* need:: COMP_LIBC_0012 */

enum {
    WCTYPE_NONE = 0u,
    WCTYPE_ALNUM = 1u,
    WCTYPE_ALPHA = 2u,
    WCTYPE_BLANK = 3u,
    WCTYPE_CNTRL = 4u,
    WCTYPE_DIGIT = 5u,
    WCTYPE_GRAPH = 6u,
    WCTYPE_LOWER = 7u,
    WCTYPE_PRINT = 8u,
    WCTYPE_PUNCT = 9u,
    WCTYPE_SPACE = 10u,
    WCTYPE_UPPER = 11u,
    WCTYPE_XDIGIT = 12u,
};

enum {
    WCTRANS_NONE = 0u,
    WCTRANS_TOLOWER = 1u,
    WCTRANS_TOUPPER = 2u,
};

static int is_valid_unicode(uint32_t wc) {
    return wc <= 0x10FFFFu && (wc < 0xD800u || wc > 0xDFFFu);
}

static int u_is_upper(uint32_t wc) {
    if (wc >= 'A' && wc <= 'Z') {
        return 1;
    }
    if ((wc >= 0x00C0u && wc <= 0x00D6u) || (wc >= 0x00D8u && wc <= 0x00DEu)) {
        return 1;
    }
    if (wc >= 0x0100u && wc <= 0x0136u && (wc & 1u) == 0u) {
        return 1;
    }
    if (wc >= 0x0139u && wc <= 0x0147u && (wc & 1u) == 1u) {
        return 1;
    }
    if (wc >= 0x014Au && wc <= 0x0176u && (wc & 1u) == 0u) {
        return 1;
    }
    if (wc == 0x0178u || (wc >= 0x0179u && wc <= 0x017Du && (wc & 1u) == 1u)) {
        return 1;
    }
    if ((wc >= 0x0391u && wc <= 0x03A1u) || (wc >= 0x03A3u && wc <= 0x03ABu)) {
        return 1;
    }
    if (wc >= 0x0400u && wc <= 0x042Fu) {
        return 1;
    }
    return 0;
}

static int u_is_lower(uint32_t wc) {
    if (wc >= 'a' && wc <= 'z') {
        return 1;
    }
    if ((wc >= 0x00DFu && wc <= 0x00F6u) || (wc >= 0x00F8u && wc <= 0x00FFu)) {
        return 1;
    }
    if (wc >= 0x0101u && wc <= 0x0137u && (wc & 1u) == 1u) {
        return 1;
    }
    if (wc >= 0x013Au && wc <= 0x0148u && (wc & 1u) == 0u) {
        return 1;
    }
    if (wc >= 0x014Bu && wc <= 0x0177u && (wc & 1u) == 1u) {
        return 1;
    }
    if (wc >= 0x017Au && wc <= 0x017Eu && (wc & 1u) == 0u) {
        return 1;
    }
    if ((wc >= 0x03B1u && wc <= 0x03C1u) || (wc >= 0x03C3u && wc <= 0x03CBu) ||
        wc == 0x03C2u) {
        return 1;
    }
    if (wc >= 0x0430u && wc <= 0x045Fu) {
        return 1;
    }
    return 0;
}

static int u_is_alpha(uint32_t wc) {
    if (u_is_upper(wc) || u_is_lower(wc)) {
        return 1;
    }
    if (wc == 0x00AAu || wc == 0x00B5u || wc == 0x00BAu) {
        return 1;
    }
    if ((wc >= 0x3041u && wc <= 0x3096u) || (wc >= 0x30A1u && wc <= 0x30FAu) ||
        (wc >= 0x4E00u && wc <= 0x9FFFu)) {
        return 1;
    }
    return 0;
}

static int u_is_space(uint32_t wc) {
    if (wc == ' ' || (wc >= '\t' && wc <= '\r')) {
        return 1;
    }
    return wc == 0x00A0u || wc == 0x1680u || (wc >= 0x2000u && wc <= 0x200Au) ||
           wc == 0x2028u || wc == 0x2029u || wc == 0x202Fu || wc == 0x205Fu ||
           wc == 0x3000u;
}

static int u_is_blank(uint32_t wc) {
    if (wc == ' ' || wc == '\t') {
        return 1;
    }
    return wc == 0x00A0u || wc == 0x1680u || (wc >= 0x2000u && wc <= 0x200Au) ||
           wc == 0x202Fu || wc == 0x205Fu || wc == 0x3000u;
}

static int u_is_cntrl(uint32_t wc) {
    return wc <= 0x1Fu || (wc >= 0x7Fu && wc <= 0x9Fu);
}

static int u_is_print(uint32_t wc) {
    if (!is_valid_unicode(wc)) {
        return 0;
    }
    return !u_is_cntrl(wc);
}

static int u_is_graph(uint32_t wc) {
    return u_is_print(wc) && !u_is_space(wc);
}

static int u_is_punct(uint32_t wc) {
    if (wc < 0x80u) {
        return (wc >= 0x21u && wc <= 0x7Eu) &&
               !((wc >= '0' && wc <= '9') || (wc >= 'A' && wc <= 'Z') ||
                 (wc >= 'a' && wc <= 'z'));
    }
    return u_is_graph(wc) && !u_is_alpha(wc);
}

static uint32_t u_tolower(uint32_t wc) {
    if (wc >= 'A' && wc <= 'Z') {
        return wc + ('a' - 'A');
    }
    if ((wc >= 0x00C0u && wc <= 0x00D6u) || (wc >= 0x00D8u && wc <= 0x00DEu)) {
        return wc + 0x20u;
    }
    if (wc >= 0x0100u && wc <= 0x0136u && (wc & 1u) == 0u) {
        return wc + 1u;
    }
    if (wc >= 0x0139u && wc <= 0x0147u && (wc & 1u) == 1u) {
        return wc + 1u;
    }
    if (wc >= 0x014Au && wc <= 0x0176u && (wc & 1u) == 0u) {
        return wc + 1u;
    }
    if (wc == 0x0178u) {
        return 0x00FFu;
    }
    if (wc >= 0x0179u && wc <= 0x017Du && (wc & 1u) == 1u) {
        return wc + 1u;
    }
    if ((wc >= 0x0391u && wc <= 0x03A1u) || (wc >= 0x03A3u && wc <= 0x03ABu)) {
        return wc + 0x20u;
    }
    if (wc >= 0x0400u && wc <= 0x040Fu) {
        return wc + 0x50u;
    }
    if (wc >= 0x0410u && wc <= 0x042Fu) {
        return wc + 0x20u;
    }
    return wc;
}

static uint32_t u_toupper(uint32_t wc) {
    if (wc >= 'a' && wc <= 'z') {
        return wc - ('a' - 'A');
    }
    if ((wc >= 0x00E0u && wc <= 0x00F6u) || (wc >= 0x00F8u && wc <= 0x00FEu)) {
        return wc - 0x20u;
    }
    if (wc == 0x00FFu) {
        return 0x0178u;
    }
    if (wc >= 0x0101u && wc <= 0x0137u && (wc & 1u) == 1u) {
        return wc - 1u;
    }
    if (wc >= 0x013Au && wc <= 0x0148u && (wc & 1u) == 0u) {
        return wc - 1u;
    }
    if (wc >= 0x014Bu && wc <= 0x0177u && (wc & 1u) == 1u) {
        return wc - 1u;
    }
    if (wc >= 0x017Au && wc <= 0x017Eu && (wc & 1u) == 0u) {
        return wc - 1u;
    }
    if ((wc >= 0x03B1u && wc <= 0x03C1u) || (wc >= 0x03C3u && wc <= 0x03CBu)) {
        return wc - 0x20u;
    }
    if (wc == 0x03C2u) {
        return 0x03A3u;
    }
    if (wc >= 0x0430u && wc <= 0x044Fu) {
        return wc - 0x20u;
    }
    if (wc >= 0x0450u && wc <= 0x045Fu) {
        return wc - 0x50u;
    }
    return wc;
}

static int c_classify(uint32_t wc, wctype_t cls) {
    if (wc >= 0x80u) {
        return 0;
    }
    switch (cls) {
    case WCTYPE_UPPER:
        return (wc >= 'A' && wc <= 'Z');
    case WCTYPE_LOWER:
        return (wc >= 'a' && wc <= 'z');
    case WCTYPE_ALPHA:
        return (wc >= 'A' && wc <= 'Z') || (wc >= 'a' && wc <= 'z');
    case WCTYPE_DIGIT:
        return (wc >= '0' && wc <= '9');
    case WCTYPE_ALNUM:
        return (wc >= 'A' && wc <= 'Z') || (wc >= 'a' && wc <= 'z') ||
               (wc >= '0' && wc <= '9');
    case WCTYPE_XDIGIT:
        return (wc >= '0' && wc <= '9') || (wc >= 'a' && wc <= 'f') ||
               (wc >= 'A' && wc <= 'F');
    case WCTYPE_BLANK:
        return (wc == ' ' || wc == '\t');
    case WCTYPE_SPACE:
        return (wc == ' ' || (wc >= '\t' && wc <= '\r'));
    case WCTYPE_CNTRL:
        return (wc <= 0x1Fu || wc == 0x7Fu);
    case WCTYPE_PRINT:
        return (wc >= 0x20u && wc <= 0x7Eu);
    case WCTYPE_GRAPH:
        return (wc >= 0x21u && wc <= 0x7Eu);
    case WCTYPE_PUNCT:
        return (wc >= 0x21u && wc <= 0x7Eu) && !c_classify(wc, WCTYPE_ALNUM);
    default:
        return 0;
    }
}

static int classify_wide(wint_t wc, wctype_t cls, locale_t loc) {
    if (wc == WEOF || wc < 0) {
        return 0;
    }
    uint32_t cp = (uint32_t)wc;
    if (cp < 0x80u || !__libc_locale_is_utf8(loc)) {
        return c_classify(cp, cls);
    }
    if (!is_valid_unicode(cp)) {
        return 0;
    }
    switch (cls) {
    case WCTYPE_UPPER:
        return u_is_upper(cp);
    case WCTYPE_LOWER:
        return u_is_lower(cp);
    case WCTYPE_ALPHA:
        return u_is_alpha(cp);
    case WCTYPE_DIGIT:
        return 0;
    case WCTYPE_ALNUM:
        return u_is_alpha(cp);
    case WCTYPE_XDIGIT:
        return 0;
    case WCTYPE_BLANK:
        return u_is_blank(cp);
    case WCTYPE_SPACE:
        return u_is_space(cp);
    case WCTYPE_CNTRL:
        return u_is_cntrl(cp);
    case WCTYPE_PRINT:
        return u_is_print(cp);
    case WCTYPE_GRAPH:
        return u_is_graph(cp);
    case WCTYPE_PUNCT:
        return u_is_punct(cp);
    default:
        return 0;
    }
}

int iswalnum_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_ALNUM, locale);
}

int iswalnum(wint_t wc) {
    return iswalnum_l(wc, (locale_t)0);
}

int iswalpha_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_ALPHA, locale);
}

int iswalpha(wint_t wc) {
    return iswalpha_l(wc, (locale_t)0);
}

int iswblank_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_BLANK, locale);
}

int iswblank(wint_t wc) {
    return iswblank_l(wc, (locale_t)0);
}

int iswcntrl_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_CNTRL, locale);
}

int iswcntrl(wint_t wc) {
    return iswcntrl_l(wc, (locale_t)0);
}

int iswdigit_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_DIGIT, locale);
}

int iswdigit(wint_t wc) {
    return iswdigit_l(wc, (locale_t)0);
}

int iswgraph_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_GRAPH, locale);
}

int iswgraph(wint_t wc) {
    return iswgraph_l(wc, (locale_t)0);
}

int iswlower_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_LOWER, locale);
}

int iswlower(wint_t wc) {
    return iswlower_l(wc, (locale_t)0);
}

int iswprint_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_PRINT, locale);
}

int iswprint(wint_t wc) {
    return iswprint_l(wc, (locale_t)0);
}

int iswpunct_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_PUNCT, locale);
}

int iswpunct(wint_t wc) {
    return iswpunct_l(wc, (locale_t)0);
}

int iswspace_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_SPACE, locale);
}

int iswspace(wint_t wc) {
    return iswspace_l(wc, (locale_t)0);
}

int iswupper_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_UPPER, locale);
}

int iswupper(wint_t wc) {
    return iswupper_l(wc, (locale_t)0);
}

int iswxdigit_l(wint_t wc, locale_t locale) {
    return classify_wide(wc, WCTYPE_XDIGIT, locale);
}

int iswxdigit(wint_t wc) {
    return iswxdigit_l(wc, (locale_t)0);
}

wctype_t wctype_l(const char *property, locale_t locale) {
    (void)locale;
    if (!property) {
        return WCTYPE_NONE;
    }
    if (strcmp(property, "alnum") == 0) {
        return WCTYPE_ALNUM;
    }
    if (strcmp(property, "alpha") == 0) {
        return WCTYPE_ALPHA;
    }
    if (strcmp(property, "blank") == 0) {
        return WCTYPE_BLANK;
    }
    if (strcmp(property, "cntrl") == 0) {
        return WCTYPE_CNTRL;
    }
    if (strcmp(property, "digit") == 0) {
        return WCTYPE_DIGIT;
    }
    if (strcmp(property, "graph") == 0) {
        return WCTYPE_GRAPH;
    }
    if (strcmp(property, "lower") == 0) {
        return WCTYPE_LOWER;
    }
    if (strcmp(property, "print") == 0) {
        return WCTYPE_PRINT;
    }
    if (strcmp(property, "punct") == 0) {
        return WCTYPE_PUNCT;
    }
    if (strcmp(property, "space") == 0) {
        return WCTYPE_SPACE;
    }
    if (strcmp(property, "upper") == 0) {
        return WCTYPE_UPPER;
    }
    if (strcmp(property, "xdigit") == 0) {
        return WCTYPE_XDIGIT;
    }
    return WCTYPE_NONE;
}

wctype_t wctype(const char *property) {
    return wctype_l(property, (locale_t)0);
}

int iswctype_l(wint_t wc, wctype_t charclass, locale_t locale) {
    return classify_wide(wc, charclass, locale);
}

int iswctype(wint_t wc, wctype_t charclass) {
    return iswctype_l(wc, charclass, (locale_t)0);
}

wint_t towlower_l(wint_t wc, locale_t locale) {
    if (wc == WEOF || wc < 0) {
        return wc;
    }
    if (!__libc_locale_is_utf8(locale)) {
        return (wc >= 'A' && wc <= 'Z') ? (wc + ('a' - 'A')) : wc;
    }
    return (wint_t)u_tolower((uint32_t)wc);
}

wint_t towlower(wint_t wc) {
    return towlower_l(wc, (locale_t)0);
}

wint_t towupper_l(wint_t wc, locale_t locale) {
    if (wc == WEOF || wc < 0) {
        return wc;
    }
    if (!__libc_locale_is_utf8(locale)) {
        return (wc >= 'a' && wc <= 'z') ? (wc - ('a' - 'A')) : wc;
    }
    return (wint_t)u_toupper((uint32_t)wc);
}

wint_t towupper(wint_t wc) {
    return towupper_l(wc, (locale_t)0);
}

wctrans_t wctrans_l(const char *charclass, locale_t locale) {
    (void)locale;
    if (!charclass) {
        return WCTRANS_NONE;
    }
    if (strcmp(charclass, "tolower") == 0) {
        return WCTRANS_TOLOWER;
    }
    if (strcmp(charclass, "toupper") == 0) {
        return WCTRANS_TOUPPER;
    }
    return WCTRANS_NONE;
}

wctrans_t wctrans(const char *charclass) {
    return wctrans_l(charclass, (locale_t)0);
}

wint_t towctrans_l(wint_t wc, wctrans_t desc, locale_t locale) {
    if (desc == WCTRANS_TOLOWER) {
        return towlower_l(wc, locale);
    }
    if (desc == WCTRANS_TOUPPER) {
        return towupper_l(wc, locale);
    }
    return wc;
}

wint_t towctrans(wint_t wc, wctrans_t desc) {
    return towctrans_l(wc, desc, (locale_t)0);
}
