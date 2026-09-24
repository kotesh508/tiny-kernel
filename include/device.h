#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include "resource.h"

#define DEVICE_NAME_MAX   32
#define DEVICE_COMPAT_MAX 64

#define DEVICE_MAX        16
#define DRIVER_MAX        16

struct fdt_header_info;

struct device;
struct driver;

typedef int (*driver_probe_t)(struct device *dev);
typedef int (*driver_remove_t)(struct device *dev);

struct device {
    const char *name;
    const char *compatible;

    struct resource resources[RESOURCE_MAX_PER_DEVICE];
    uint32_t resource_count;

    void *driver_data;
    struct driver *driver;

    enum device_state {
        DEVICE_UNREGISTERED = 0,
        DEVICE_REGISTERED,
        DEVICE_BOUND,
        DEVICE_UNBOUND
    } state;
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

int device_add_resource(
    struct device *dev,
    enum resource_type type,
    uintptr_t start,
    uintptr_t end,
    uint32_t flags);

struct resource *device_get_resource(
    struct device *dev,
    enum resource_type type,
    uint32_t index);

/* DTB -> device creation */
int device_discover_from_fdt_reg(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const char *name,
    const char *compatible);

int device_discover_from_fdt(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const char *name,
    const char *compatible);

struct device *device_find_compatible(const char *compatible);

struct fdt_device_desc {
    const char *name;
    const char *compatible;
};

int device_discover_from_fdt_list(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const struct fdt_device_desc *desc,
    uint32_t count);

int device_model_test(void);
int device_lifecycle_test(void);
int device_hardening_test(void);

int device_multi_test(
    uintptr_t dtb,
    const struct fdt_header_info *info);


#endif
