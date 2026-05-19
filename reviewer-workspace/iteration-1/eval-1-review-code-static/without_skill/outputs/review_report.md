# Code Review Report

**File:** `buggy.c`  
**Reviewer:** baseline (without_skill)  
**Date:** 2026-05-19  

---

## Summary

A total of **7 defects** found (3 critical, 2 high, 2 medium).

---

## Defects

### [CRITICAL] 1. Stack Buffer Overflow — `strcpy(buf, input)` (line 24)

- **Location:** `buggy.c:24` — `process_records()`
- **Description:** `buf` is 64 bytes (`MAX_BUF = 64`). `input` comes from `argv[1]` (entirely user-controlled). No length check is performed. An input longer than 63 characters will overflow the stack buffer, corrupting adjacent stack frames (including the return address).
- **Risk:** Remote code execution / crash via controlled input.
- **Fix:** Use `strncpy(buf, input, sizeof(buf) - 1); buf[sizeof(buf) - 1] = '\0';` or `snprintf(buf, sizeof(buf), "%s", input);`.

---

### [CRITICAL] 2. Stack Buffer Overflow — `strcpy(r->name, name)` (line 16)

- **Location:** `buggy.c:16` — `create_record()`
- **Description:** `r->name` is a fixed 32-byte array. `name` (derived from a parsed token) is copied via `strcpy` without length checking. If a token is >= 32 bytes, it overflows `name` into adjacent heap memory (specifically the `data` pointer field, enabling pointer overwrite attacks).
- **Risk:** Heap corruption, arbitrary write primitive.
- **Fix:** Use `strncpy(r->name, name, sizeof(r->name) - 1); r->name[sizeof(r->name) - 1] = '\0';` or switch to `snprintf`.

---

### [CRITICAL] 3. Heap Array Out-of-Bounds Write (line 31-32)

- **Location:** `buggy.c:31-32` — `process_records()`
- **Description:** `recs` is a fixed-size array of 10 pointers on the stack. The `while` loop parses tokens and assigns `recs[count]` without checking `count < 10`. An input with >= 11 comma-separated tokens will write past the end of the array.
- **Risk:** Stack corruption, crash, potential code execution.
- **Fix:** Add a bounds check: `if (count >= 10) break;` before `recs[count] = create_record(count, token);`.

---

### [HIGH] 4. NULL Pointer Dereference — no NULL check on `malloc` (line 14)

- **Location:** `buggy.c:14-18` — `create_record()`
- **Description:** `malloc(sizeof(Record))` can return NULL on allocation failure. `r->id = id` (line 15) unconditionally dereferences the return value. Same issue for `r->data = (char*)malloc(256)` on line 17.
- **Risk:** Crash on low-memory conditions.
- **Fix:** Check `if (r == NULL) return NULL;` after each `malloc` call.

---

### [HIGH] 5. Memory Leak — allocated records never freed (all paths)

- **Location:** `buggy.c:13-20`, `buggy.c:22-38` — `create_record()` / `process_records()`
- **Description:** Each call to `create_record()` allocates two heap blocks (`Record` struct + `data` buffer). Neither `process_records()` nor `main()` frees any of them. All 10+ records leak on every invocation.
- **Risk:** Resource exhaustion in long-running or repeated invocations.
- **Fix:** Add a cleanup loop in `process_records()` before returning: `for (int i = 0; i < count; i++) { free(recs[i]->data); free(recs[i]); }`.

---

### [MEDIUM] 6. NULL Pointer Dereference in print loop (line 37)

- **Location:** `buggy.c:37` — `process_records()`
- **Description:** If any `create_record` call returned NULL (due to allocation failure), `recs[i]` would be NULL, and `recs[i]->id` would segfault.
- **Risk:** Crash.
- **Fix:** Add NULL guard: `if (recs[i] != NULL) { ... }`.

---

### [MEDIUM] 7. `sprintf` instead of `snprintf` (line 18)

- **Location:** `buggy.c:18` — `create_record()`
- **Description:** While this specific call is bounded ("Record-%d-data" fits within 256 bytes), `sprintf` provides no protection if the format string or arguments are later modified. `strtok` usage (line 29,33) is also not thread-safe.
- **Risk:** Low in current form; latent bug if code evolves.
- **Fix:** Use `snprintf(r->data, 256, "Record-%d-data", id);`.

---

## Severity Distribution

| Severity | Count |
|----------|-------|
| CRITICAL | 3     |
| HIGH     | 2     |
| MEDIUM   | 2     |
| LOW      | 0     |

---

## Recommendations

1. Replace all `strcpy` / `sprintf` with `strncpy` / `snprintf` (or the MSVC-safe `_s` variants).
2. Add array bounds checks before every write into `recs[]`.
3. Add NULL-pointer checks after every heap allocation.
4. Add a memory cleanup / free path before every function return.
5. Consider removing the arbitrary 256-byte allocation for `data` in favor of computing the exact length.
