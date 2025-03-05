#ifndef BT_SPP_H
#define BT_SPP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*recv_callback_t)(char *, size_t);
void set_recv_callback(recv_callback_t func);
void btstack_main();

size_t spp_write(void *buffer, size_t size, size_t count);
size_t spp_read(void *buffer, size_t size, size_t count);

#ifdef __cplusplus
}
#endif

#endif // BT_SPP_H
