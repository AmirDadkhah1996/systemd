/* SPDX-License-Identifier: LGPL-2.1+ */

#include <net/if.h>

#include "alloc-util.h"
#include "bus-common-errors.h"
#include "bus-util.h"
#include "networkd-link-bus.h"
#include "networkd-link.h"
#include "networkd-manager-bus.h"
#include "networkd-manager.h"
#include "path-util.h"
#include "strv.h"

static int method_list_links(sd_bus_message *message, void *userdata, sd_bus_error *error) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        Manager *manager = userdata;
        Iterator i;
        Link *link;
        int r;

        r = sd_bus_message_new_method_return(message, &reply);
        if (r < 0)
                return r;

        r = sd_bus_message_open_container(reply, 'a', "(iso)");
        if (r < 0)
                return r;

        HASHMAP_FOREACH(link, manager->links, i) {
                _cleanup_free_ char *path = NULL;

                path = link_bus_path(link);
                if (!path)
                        return -ENOMEM;

                r = sd_bus_message_append(
                        reply, "(iso)",
                        link->ifindex,
                        link->ifname,
                        empty_to_root(path));
                if (r < 0)
                        return r;
        }

        r = sd_bus_message_close_container(reply);
        if (r < 0)
                return r;

        return sd_bus_send(NULL, reply, NULL);
}

static int method_get_link_by_name(sd_bus_message *message, void *userdata, sd_bus_error *error) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        _cleanup_free_ char *path = NULL;
        Manager *manager = userdata;
        const char *name;
        int index, r;
        Link *link;

        r = sd_bus_message_read(message, "s", &name);
        if (r < 0)
                return r;

        index = if_nametoindex(name);
        if (index <= 0)
                return sd_bus_error_setf(error, BUS_ERROR_NO_SUCH_LINK, "Link %s not known", name);

        link = hashmap_get(manager->links, INT_TO_PTR(index));
        if (!link)
                return sd_bus_error_setf(error, BUS_ERROR_NO_SUCH_LINK, "Link %s not known", name);

        r = sd_bus_message_new_method_return(message, &reply);
        if (r < 0)
                return r;

        path = link_bus_path(link);
        if (!path)
                return -ENOMEM;

        r = sd_bus_message_append(reply, "io", link->ifindex, empty_to_root(path));
        if (r < 0)
                return r;

        return sd_bus_send(NULL, reply, NULL);
}

static int method_get_link_by_index(sd_bus_message *message, void *userdata, sd_bus_error *error) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        _cleanup_free_ char *path = NULL;
        Manager *manager = userdata;
        int32_t index;
        Link *link;
        int r;

        r = sd_bus_message_read(message, "i", &index);
        if (r < 0)
                return r;

        link = hashmap_get(manager->links, INT_TO_PTR((int) index));
        if (!link)
                return sd_bus_error_setf(error, BUS_ERROR_NO_SUCH_LINK, "Link %" PRIi32 " not known", index);

        r = sd_bus_message_new_method_return(message, &reply);
        if (r < 0)
                return r;

        path = link_bus_path(link);
        if (!path)
                return -ENOMEM;

        r = sd_bus_message_append(reply, "so", link->ifname, empty_to_root(path));
        if (r < 0)
                return r;

        return sd_bus_send(NULL, reply, NULL);
}

const sd_bus_vtable manager_vtable[] = {
        SD_BUS_VTABLE_START(0),

        SD_BUS_PROPERTY("OperationalState", "s", property_get_operational_state, offsetof(Manager, operational_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("CarrierState", "s", property_get_carrier_state, offsetof(Manager, carrier_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("AddressState", "s", property_get_address_state, offsetof(Manager, address_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),

        SD_BUS_METHOD("ListLinks", NULL, "a(iso)", method_list_links, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("GetLinkByName", "s", "io", method_get_link_by_name, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("GetLinkByIndex", "i", "so", method_get_link_by_index, SD_BUS_VTABLE_UNPRIVILEGED),

        SD_BUS_VTABLE_END
};

int manager_send_changed_strv(Manager *manager, char **properties) {
        assert(manager);
        assert(properties);

        if (!manager->bus)
                return 0;

        return sd_bus_emit_properties_changed_strv(
                        manager->bus,
                        "/org/freedesktop/network1",
                        "org.freedesktop.network1.Manager",
                        properties);
}
