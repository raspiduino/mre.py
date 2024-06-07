// Standard headers
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Micropython headers
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/frozenmod.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "shared/readline/readline.h"
#include "shared/runtime/pyexec.h"
#include "genhdr/mpversion.h"

// MRE headers
#include "vmtimer.h"
#include "console/Console_io.h"

// Micropython stuffs
static char *stack_top;
static char heap[MICROPY_HEAP_SIZE];

#if MICROPY_REPL_INFO
static bool repl_display_debugging_info = 0;
#endif

#define EXEC_FLAG_PRINT_EOF             (1 << 0)
#define EXEC_FLAG_ALLOW_DEBUGGING       (1 << 1)
#define EXEC_FLAG_IS_REPL               (1 << 2)
#define EXEC_FLAG_SOURCE_IS_RAW_CODE    (1 << 3)
#define EXEC_FLAG_SOURCE_IS_VSTR        (1 << 4)
#define EXEC_FLAG_SOURCE_IS_FILENAME    (1 << 5)
#define EXEC_FLAG_SOURCE_IS_READER      (1 << 6)
#define EXEC_FLAG_NO_INTERRUPT          (1 << 7)

// parses, compiles and executes the code in the lexer
// frees the lexer before returning
// EXEC_FLAG_PRINT_EOF prints 2 EOF chars: 1 after normal output, 1 after exception output
// EXEC_FLAG_ALLOW_DEBUGGING allows debugging info to be printed after executing the code
// EXEC_FLAG_IS_REPL is used for REPL inputs (flag passed on to mp_compile)
static int parse_compile_execute(const void *source, mp_parse_input_kind_t input_kind, mp_uint_t exec_flags) {
    int ret = 0;
    #if MICROPY_REPL_INFO
    uint32_t start = 0;
    #endif

    #ifdef MICROPY_BOARD_BEFORE_PYTHON_EXEC
    MICROPY_BOARD_BEFORE_PYTHON_EXEC(input_kind, exec_flags);
    #endif

    // by default a SystemExit exception returns 0
    pyexec_system_exit = 0;

    nlr_buf_t nlr;
    nlr.ret_val = NULL;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t module_fun;
        #if MICROPY_MODULE_FROZEN_MPY
        if (exec_flags & EXEC_FLAG_SOURCE_IS_RAW_CODE) {
            // source is a raw_code object, create the function
            const mp_frozen_module_t *frozen = source;
            mp_module_context_t *ctx = m_new_obj(mp_module_context_t);
            ctx->module.globals = mp_globals_get();
            ctx->constants = frozen->constants;
            module_fun = mp_make_function_from_proto_fun(frozen->proto_fun, ctx, NULL);
        } else
        #endif
        {
            #if MICROPY_ENABLE_COMPILER
            mp_lexer_t *lex;
            if (exec_flags & EXEC_FLAG_SOURCE_IS_VSTR) {
                const vstr_t *vstr = source;
                lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, vstr->buf, vstr->len, 0);
            } else if (exec_flags & EXEC_FLAG_SOURCE_IS_READER) {
                lex = mp_lexer_new(MP_QSTR__lt_stdin_gt_, *(mp_reader_t *)source);
            } else if (exec_flags & EXEC_FLAG_SOURCE_IS_FILENAME) {
                lex = mp_lexer_new_from_file(qstr_from_str(source));
            } else {
                lex = (mp_lexer_t *)source;
            }
            // source is a lexer, parse and compile the script
            qstr source_name = lex->source_name;
            mp_parse_tree_t parse_tree = mp_parse(lex, input_kind);
            module_fun = mp_compile(&parse_tree, source_name, exec_flags & EXEC_FLAG_IS_REPL);
            #else
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("script compilation not supported"));
            #endif
        }

        // execute code
        if (!(exec_flags & EXEC_FLAG_NO_INTERRUPT)) {
            mp_hal_set_interrupt_char(CHAR_CTRL_C);
        }
        #if MICROPY_REPL_INFO
        start = mp_hal_ticks_ms();
        #endif
        mp_call_function_0(module_fun);
        mp_hal_set_interrupt_char(-1); // disable interrupt
        mp_handle_pending(true); // handle any pending exceptions (and any callbacks)
        nlr_pop();
        ret = 1;
        if (exec_flags & EXEC_FLAG_PRINT_EOF) {
            mp_hal_stdout_tx_strn("\x04", 1);
        }
    } else {
        // uncaught exception
        mp_hal_set_interrupt_char(-1); // disable interrupt
        mp_handle_pending(false); // clear any pending exceptions (and run any callbacks)

        if (exec_flags & EXEC_FLAG_SOURCE_IS_READER) {
            const mp_reader_t *reader = source;
            reader->close(reader->data);
        }

        // print EOF after normal output
        if (exec_flags & EXEC_FLAG_PRINT_EOF) {
            mp_hal_stdout_tx_strn("\x04", 1);
        }

        // check for SystemExit
        if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(((mp_obj_base_t *)nlr.ret_val)->type), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
            // at the moment, the value of SystemExit is unused
            ret = pyexec_system_exit;
        } else {
            mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
            ret = 0;
        }
    }

    #if MICROPY_REPL_INFO
    // display debugging info if wanted
    if ((exec_flags & EXEC_FLAG_ALLOW_DEBUGGING) && repl_display_debugging_info) {
        mp_uint_t ticks = mp_hal_ticks_ms() - start; // TODO implement a function that does this properly
        mp_printf(&mp_plat_print, "took " UINT_FMT " ms\n", ticks);
        // qstr info
        {
            size_t n_pool, n_qstr, n_str_data_bytes, n_total_bytes;
            qstr_pool_info(&n_pool, &n_qstr, &n_str_data_bytes, &n_total_bytes);
            mp_printf(&mp_plat_print, "qstr:\n  n_pool=%u\n  n_qstr=%u\n  "
                "n_str_data_bytes=%u\n  n_total_bytes=%u\n",
                (unsigned)n_pool, (unsigned)n_qstr, (unsigned)n_str_data_bytes, (unsigned)n_total_bytes);
        }

        #if MICROPY_ENABLE_GC
        // run collection and print GC info
        gc_collect();
        gc_dump_info(&mp_plat_print);
        #endif
    }
    #endif

    if (exec_flags & EXEC_FLAG_PRINT_EOF) {
        mp_hal_stdout_tx_strn("\x04", 1);
    }

    #ifdef MICROPY_BOARD_AFTER_PYTHON_EXEC
    MICROPY_BOARD_AFTER_PYTHON_EXEC(input_kind, exec_flags, nlr.ret_val, &ret);
    #endif

    return ret;
}

vstr_t line_new;
vstr_t line;
bool upy_repl_main = true;
mp_parse_input_kind_t parse_input_kind = MP_PARSE_SINGLE_INPUT;

void upy_repl_input_restart() {
    // If the GC is locked at this point there is no way out except a reset,
    // so force the GC to be unlocked to help the user debug what went wrong.
    if (MP_STATE_THREAD(gc_lock_depth) != 0) {
        MP_STATE_THREAD(gc_lock_depth) = 0;
    }

    line = line_new;
    vstr_reset(&line);
    readline_init(&line, mp_repl_get_ps1());
}

void upy_repl_friendly_repl_reset() {
    mp_hal_stdout_tx_str(MICROPY_BANNER_NAME_AND_VERSION);
    mp_hal_stdout_tx_str("; " MICROPY_BANNER_MACHINE);
    mp_hal_stdout_tx_str("\r\n");
    #if MICROPY_PY_BUILTINS_HELP
    mp_hal_stdout_tx_str("Type \"help()\" for more information.\r\n");
    #endif

    upy_repl_input_restart();
}

void upy_repl_init() {
    vstr_init(&line, 32);
    upy_repl_friendly_repl_reset();
}

void upy_repl_rx_cont_end() {
    // Continue the rest of original upy_repl_rx_main()
    int ret = parse_compile_execute(&line, parse_input_kind, EXEC_FLAG_ALLOW_DEBUGGING | EXEC_FLAG_IS_REPL | EXEC_FLAG_SOURCE_IS_VSTR);
    if (ret & PYEXEC_FORCED_EXIT) {
        vm_exit_app();
        return;
    }

    // Done cont mode, back to main mode
    upy_repl_main = true;
    upy_repl_input_restart(); 
}

void upy_repl_rx_cont(char c) {
    int ret = readline_process_char(c);
    if (ret < 0) {
        return;
    }

    if (ret == CHAR_CTRL_C) {
        // Cancel everything
        upy_repl_main = true; // Switch back to main mode
        mp_hal_stdout_tx_str("\r\n");
        upy_repl_input_restart();
    } else if (ret == CHAR_CTRL_D) {
        // Stop entering compound statement
        upy_repl_rx_cont_end();
    }

    // If condition is still true
    if (mp_repl_continue_with_input(vstr_null_terminated_str(&line))) {
        // Prepare for the next loop
        vstr_add_byte(&line, '\n');
        readline_init(&line, mp_repl_get_ps2());
    } else {
        // condition no longer true -> break out of the loop
        upy_repl_rx_cont_end();
    }
}

void upy_repl_rx_main(char c) {
    int ret = readline_process_char(c);
    if (ret < 0) {
        return;
    }

    /*if (ret == CHAR_CTRL_A) {
        // change to raw REPL
        mp_hal_stdout_tx_str("\r\n");
        vstr_clear(&line);
        pyexec_mode_kind = PYEXEC_MODE_RAW_REPL;
        return 0;
    } else*/ if (ret == CHAR_CTRL_B) {
        // reset friendly REPL
        mp_hal_stdout_tx_str("\r\n");
        upy_repl_friendly_repl_reset();
        return;
    } else if (ret == CHAR_CTRL_C) {
        // break
        mp_hal_stdout_tx_str("\r\n");
        return;
    } else if (ret == CHAR_CTRL_D) {
        // exit for a soft reset
        mp_hal_stdout_tx_str("\r\n");
        vstr_clear(&line);
        vm_exit_app();
        return;
    } else if (ret == CHAR_CTRL_E) {
        // paste mode
        /*mp_hal_stdout_tx_str("\r\npaste mode; Ctrl-C to cancel, Ctrl-D to finish\r\n=== ");
        vstr_reset(&line);
        for (;;) {
            char c = mp_hal_stdin_rx_chr();
            if (c == CHAR_CTRL_C) {
                // cancel everything
                mp_hal_stdout_tx_str("\r\n");
                upy_repl_input_restart();
                return;
            } else if (c == CHAR_CTRL_D) {
                // end of input
                mp_hal_stdout_tx_str("\r\n");
                break;
            } else {
                // add char to buffer and echo
                vstr_add_byte(&line, c);
                if (c == '\r') {
                    mp_hal_stdout_tx_str("\r\n=== ");
                } else {
                    mp_hal_stdout_tx_strn(&c, 1);
                }
            }
        }
        parse_input_kind = MP_PARSE_FILE_INPUT;*/
    } else if (vstr_len(&line) == 0) {
        return;
    } else {
        // got a line with non-zero length, see if it needs continuing
        if (mp_repl_continue_with_input(vstr_null_terminated_str(&line))) {
            vstr_add_byte(&line, '\n');
            readline_init(&line, mp_repl_get_ps2());

            // switch to upy_repl_rx_cont()
            upy_repl_main = false;
        }
    }

    ret = parse_compile_execute(&line, parse_input_kind, EXEC_FLAG_ALLOW_DEBUGGING | EXEC_FLAG_IS_REPL | EXEC_FLAG_SOURCE_IS_VSTR);
    if (ret & PYEXEC_FORCED_EXIT) {
        vm_exit_app();
        return;
    }

    upy_repl_input_restart();
}

void upy_repl_rx(char c) {
    if (upy_repl_main) {
        // Normal input
        upy_repl_rx_main(c);
    } else {
        // Continuing input
        upy_repl_rx_cont(c);
    }
}

void upy_init() {
    // Init Micropython
    int stack_dummy;
    stack_top = (char *)&stack_dummy;

    gc_init(heap, heap + sizeof(heap));
    mp_init();
}

void upy_deinit() {
    mp_deinit();
}

void gc_collect(void) {
    // WARNING: This gc_collect implementation doesn't try to get root
    // pointers from CPU registers, and thus may function incorrectly.
    void *dummy;
    gc_collect_start();
    gc_collect_root(&dummy, ((mp_uint_t)stack_top - (mp_uint_t)&dummy) / sizeof(mp_uint_t));
    gc_collect_end();
    gc_dump_info(&mp_plat_print);
}

mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    mp_raise_OSError(MP_ENOENT);
}

mp_import_stat_t mp_import_stat(const char *path) {
    return MP_IMPORT_STAT_NO_EXIST;
}

void nlr_jump_fail(void *val) {
    while (1) {
        ;
    }
}

void NORETURN __fatal_error(const char *msg) {
    while (1) {
        ;
    }
}

void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
    printf("Assertion '%s' failed, at file %s:%d\n", expr, file, line);
    __fatal_error("Assertion failed");
}
