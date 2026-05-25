#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

/*
 * Security invariant: Symbol names must be bounded in length before being
 * copied into fixed-size symbol table fields. No symbol name, regardless
 * of how it is crafted, should be allowed to exceed the maximum safe field
 * size (SYMNAME_MAX). This test validates that a safe symbol-name copy
 * function enforces this boundary under adversarial inputs.
 */

/* Mirror the constraint that should exist in the real code */
#define SYMNAME_MAX 32   /* maximum safe symbol name length (field size - 1) */
#define SYMTAB_SIZE 512

/* Simulated symbol table structure matching the vulnerable layout */
typedef struct {
    char name[SYMNAME_MAX + 1];  /* fixed-size name field */
    int  value;
    int  defined;
    /* canary to detect overflow */
    uint32_t canary;
} symtab_entry_t;

static symtab_entry_t symtab[SYMTAB_SIZE];

#define CANARY_VALUE 0xDEADBEEFu

/* Safe symbol insertion — the invariant we are testing */
static int safe_insert_symbol(int ix, const char *name, int value)
{
    if (ix < 0 || ix >= SYMTAB_SIZE)
        return -1;
    if (name == NULL)
        return -1;
    /* INVARIANT: reject names that exceed the fixed field */
    if (strlen(name) > SYMNAME_MAX)
        return -1;

    strncpy(symtab[ix].name, name, SYMNAME_MAX);
    symtab[ix].name[SYMNAME_MAX] = '\0';
    symtab[ix].value   = value;
    symtab[ix].defined = 1;
    return 0;
}

static void setup_symtab(void)
{
    for (int i = 0; i < SYMTAB_SIZE; i++) {
        memset(symtab[i].name, 0, sizeof(symtab[i].name));
        symtab[i].value   = 0;
        symtab[i].defined = 0;
        symtab[i].canary  = CANARY_VALUE;
    }
}

/* ------------------------------------------------------------------ */

START_TEST(test_symbol_name_length_invariant)
{
    /* Invariant: symbol names longer than SYMNAME_MAX must never be
     * written into the fixed-size name field; the canary adjacent to
     * every entry must remain intact after any insertion attempt.      */

    const char *payloads[] = {
        /* exact boundary */
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345",   /* 32 chars == SYMNAME_MAX */
        /* one over boundary */
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456",  /* 33 chars */
        /* classic buffer-overflow lengths */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 64 */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 256 */
        /* null-byte tricks (C string stops at first NUL, but test the wrapper) */
        "",                                   /* empty string */
        /* format-string-like payloads */
        "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
        "%n%n%n%n%n%n%n%n",
        /* path traversal / shell metacharacters embedded in long name */
        "../../../../etc/passwd_AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        /* all printable ASCII repeated */
        "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
        /* whitespace / control characters */
        "\t\n\r\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f",
        /* Unicode-like multi-byte sequences (treated as raw bytes in C) */
        "\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf",
        /* repeated special chars */
        ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;",
        /* NOP-sled-like payload */
        "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90",
        /* valid short names (must succeed) */
        "A",
        "LOOP",
        "START",
        "END",
        "LABEL1",
    };

    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        setup_symtab();

        int slot = 0;
        int ret  = safe_insert_symbol(slot, payloads[i], i);

        size_t plen = strlen(payloads[i]);

        if (plen > SYMNAME_MAX) {
            /* INVARIANT 1: oversized names must be rejected */
            ck_assert_msg(ret != 0,
                "Payload[%d] (len=%zu) was accepted but should have been "
                "rejected (exceeds SYMNAME_MAX=%d)",
                i, plen, SYMNAME_MAX);
        } else {
            /* INVARIANT 2: valid-length names must be accepted */
            ck_assert_msg(ret == 0,
                "Payload[%d] (len=%zu) was rejected but should have been "
                "accepted (within SYMNAME_MAX=%d)",
                i, plen, SYMNAME_MAX);

            /* INVARIANT 3: stored name must be NUL-terminated within field */
            ck_assert_msg(symtab[slot].name[SYMNAME_MAX] == '\0',
                "Payload[%d]: name field is not NUL-terminated at position %d",
                i, SYMNAME_MAX);

            /* INVARIANT 4: stored name must match the original */
            ck_assert_msg(strncmp(symtab[slot].name, payloads[i], SYMNAME_MAX) == 0,
                "Payload[%d]: stored name does not match original", i);
        }

        /* INVARIANT 5: canary of every slot must be intact (no overflow) */
        for (int s = 0; s < SYMTAB_SIZE; s++) {
            ck_assert_msg(symtab[s].canary == CANARY_VALUE,
                "Payload[%d]: canary corrupted in slot %d — heap overflow detected",
                i, s);
        }

        /* INVARIANT 6: name field length must never exceed SYMNAME_MAX */
        size_t stored_len = strnlen(symtab[slot].name, sizeof(symtab[slot].name));
        ck_assert_msg(stored_len <= SYMNAME_MAX,
            "Payload[%d]: stored name length %zu exceeds SYMNAME_MAX=%d",
            i, stored_len, SYMNAME_MAX);
    }
}
END_TEST

/* ------------------------------------------------------------------ */

START_TEST(test_symbol_name_null_input)
{
    /* INVARIANT: NULL symbol name must never crash or corrupt memory */
    setup_symtab();
    int ret = safe_insert_symbol(0, NULL, 0);
    ck_assert_msg(ret != 0, "NULL symbol name was accepted — should be rejected");

    for (int s = 0; s < SYMTAB_SIZE; s++) {
        ck_assert_msg(symtab[s].canary == CANARY_VALUE,
            "Canary corrupted in slot %d after NULL name insertion", s);
    }
}
END_TEST

START_TEST(test_symbol_name_boundary_exact)
{
    /* INVARIANT: a name of exactly SYMNAME_MAX characters must be accepted
     * and stored without overflowing the field.                          */
    setup_symtab();

    char exact[SYMNAME_MAX + 1];
    memset(exact, 'X', SYMNAME_MAX);
    exact[SYMNAME_MAX] = '\0';

    int ret = safe_insert_symbol(0, exact, 42);
    ck_assert_msg(ret == 0, "Exact-boundary name was incorrectly rejected");
    ck_assert_msg(symtab[0].name[SYMNAME_MAX] == '\0',
                  "Name field not NUL-terminated at boundary");
    ck_assert_msg(symtab[0].canary == CANARY_VALUE,
                  "Canary corrupted by exact-boundary name");
}
END_TEST

START_TEST(test_symbol_name_one_over_boundary)
{
    /* INVARIANT: a name of SYMNAME_MAX+1 characters must be rejected */
    setup_symtab();

    char over[SYMNAME_MAX + 2];
    memset(over, 'Y', SYMNAME_MAX + 1);
    over[SYMNAME_MAX + 1] = '\0';

    int ret = safe_insert_symbol(0, over, 99);
    ck_assert_msg(ret != 0, "One-over-boundary name was incorrectly accepted");

    for (int s = 0; s < SYMTAB_SIZE; s++) {
        ck_assert_msg(symtab[s].canary == CANARY_VALUE,
            "Canary corrupted in slot %d after one-over-boundary attempt", s);
    }
}
END_TEST

/* ------------------------------------------------------------------ */

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s       = suite_create("Security_SymbolNameOverflow");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_symbol_name_length_invariant);
    tcase_add_test(tc_core, test_symbol_name_null_input);
    tcase_add_test(tc_core, test_symbol_name_boundary_exact);
    tcase_add_test(tc_core, test_symbol_name_one_over_boundary);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int      number_failed;
    Suite   *s;
    SRunner *sr;

    s  = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}