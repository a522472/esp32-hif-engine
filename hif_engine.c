#include "py/dynruntime.h"

#define GPIO_OUT_W1TS_REG 0x60004008
#define GPIO_OUT_W1TC_REG 0x6000400C
#define GPIO_IN_REG       0x6000403C

// 💡 這裡換成小寫的 static 了！
static mp_obj_t hif_engine_play(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t out_bufinfo;
    mp_buffer_info_t in_bufinfo;
    
    mp_get_buffer_raise(args[0], &out_bufinfo, MP_BUFFER_READ);
    mp_get_buffer_raise(args[1], &in_bufinfo, MP_BUFFER_WRITE);
    
    uint32_t pre_delay = mp_obj_get_int(args[2]);
    uint32_t post_delay = mp_obj_get_int(args[3]);

    size_t length = out_bufinfo.len / 2;
    uint8_t *out_data = (uint8_t *)out_bufinfo.buf;
    uint32_t *in_data = (uint32_t *)in_bufinfo.buf;

    volatile uint32_t *gpio_w1ts = (volatile uint32_t *)GPIO_OUT_W1TS_REG;
    volatile uint32_t *gpio_w1tc = (volatile uint32_t *)GPIO_OUT_W1TC_REG;
    volatile uint32_t *gpio_in   = (volatile uint32_t *)GPIO_IN_REG;

    size_t out_idx = 0;
    for (size_t i = 0; i < length; i++) {
        uint8_t set_m = out_data[out_idx++];
        uint8_t clr_m = out_data[out_idx++];

        *gpio_w1tc = clr_m;
        *gpio_w1ts = set_m;

        for (volatile uint32_t d = 0; d < pre_delay; d++) {
            __asm__ __volatile__("nop");
        }

        in_data[i] = *gpio_in;

        for (volatile uint32_t d = 0; d < post_delay; d++) {
            __asm__ __volatile__("nop");
        }
    }
    return mp_const_none;
}

// 💡 這裡也換成小寫的 static！
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(hif_engine_play_obj, 4, 4, hif_engine_play);

mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    MP_DYNRUNTIME_INIT_ENTRY
    mp_store_global(MP_QSTR_play, MP_OBJ_FROM_PTR(&hif_engine_play_obj));
    MP_DYNRUNTIME_INIT_EXIT
}
