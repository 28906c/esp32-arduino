#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* True if TF card is mounted and directory structure is ready. */
bool storage_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */
