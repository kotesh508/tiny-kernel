#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>

#define DEVICE_NAME_MAX   32
#define DEVICE_COMPAT_MAX 64

#define DEVICE_MAX        16
#define DRIVER_MAX        16

enum device_state {
    DEVICE_UNREGISTERED = 0,
    DEVICE_REGISTERED,
    DEVICE_BOUND,
    DEVICE_UNBOUND
};

struct device;
struct driver;

typedef int (*driver_probe_t)(struct device *dev);
typedef int (*driver_remove_t)(struct device *dev);

struct device {
    const char *name;
    const char *compatible;

    uintptr_t base;
    uintptr_t size;
    uint32_t irq;

    void *driver_data;
    struct driver *driver;

    enum device_state state;
};

struct driver {
    const char *name;
    const char *compatible;

    driver_probe_t probe;
    driver_remove_t remove;

    uint32_t registered;
};

int device_register(struct device *dev);
int device_unregister(struct device *dev);

int driver_register(struct driver *drv);
int driver_unregister(struct driver *drv);

int device_bind(struct device *dev, struct driver *drv);
int device_unbind(struct device *dev);

int device_bind_all(void);
int device_model_test(void);

#endif
