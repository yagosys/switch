/*
Copyright 2013-present Barefoot Networks, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "switchapi/switch_base_types.h"
#include "switchapi/switch_port.h"
#include "switch_pd.h"
#include "switch_port_int.h"

static switch_port_info_t switch_port_info[SWITCH_API_MAX_PORTS];
static switch_port_info_t null_port_info;
static switch_port_info_t dummy_port_info;

switch_status_t
switch_port_init(switch_device_t device)
{
    switch_port_info_t                *port_info = NULL;
    int                                index = 0;

    memset(switch_port_info, 0, sizeof(switch_port_info_t) * SWITCH_API_MAX_PORTS);

    for (index = 0; index < SWITCH_API_MAX_PORTS; index++) {
        port_info = &switch_port_info[index];
        SWITCH_PORT_ID(port_info) = index;
        port_info->ifindex = index + 1;
        port_info->port_type = SWITCH_PORT_TYPE_NORMAL;
        if (index == CPU_PORT_ID) {
            port_info->port_type = SWITCH_PORT_TYPE_CPU;
        }
        port_info->lag_handle = 0;

#ifdef SWITCH_PD
        switch_pd_lag_group_table_add_entry(device, port_info->ifindex,
                                     SWITCH_PORT_ID(port_info),
                                     &(port_info->mbr_hdl),
                                     &(port_info->lg_entry));
        port_info->hw_entry[0] = SWITCH_HW_INVALID_HANDLE;
        port_info->hw_entry[1] = SWITCH_HW_INVALID_HANDLE;
        switch_pd_ingress_port_mapping_table_add_entry(device,
                                     SWITCH_PORT_ID(port_info),
                                     port_info->ifindex,
                                     port_info->port_type,
                                     port_info->hw_entry);
        port_info->eg_port_entry = SWITCH_HW_INVALID_HANDLE;
        switch_pd_egress_port_mapping_table_add_entry(device,
                                     SWITCH_PORT_ID(port_info),
                                     port_info->ifindex,
                                     port_info->port_type,
                                     &(port_info->eg_port_entry));
        port_info->port_handle = id_to_handle(SWITCH_HANDLE_TYPE_PORT, index);
#endif
    }
    return SWITCH_STATUS_SUCCESS;
}

switch_port_info_t *
switch_api_port_get_internal(switch_port_t port)
{
    port = handle_to_id(port);
    if(port < SWITCH_API_MAX_PORTS)
        return &switch_port_info[port];
    else if(port == NULL_PORT_ID) {
        return &null_port_info;
    }
    else {
        return &dummy_port_info;
    }
}

switch_status_t
switch_api_port_set(switch_device_t device, switch_api_port_info_t *api_port_info)
{
    switch_port_info_t *port_info = switch_api_port_get_internal(api_port_info->port_number);
    UNUSED(device);
    if (port_info) {
        // blindly overwrite the values - may need to get a modify later!
        memcpy(&(port_info->api_port_info), api_port_info, sizeof(switch_api_port_info_t));
        return SWITCH_STATUS_SUCCESS;
    }
    return SWITCH_STATUS_FAILURE;
}

switch_status_t
switch_api_port_get(switch_device_t device, switch_api_port_info_t *api_port_info)
{
    switch_port_info_t *port_info = switch_api_port_get_internal(api_port_info->port_number);
    if (!port_info) {
        api_port_info = NULL;
        return SWITCH_STATUS_FAILURE;
    }
    memcpy(api_port_info, &port_info->api_port_info, sizeof(switch_api_port_info_t));
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t
switch_api_port_delete(switch_device_t device, uint16_t port_number)
{
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_port_info_t *port_info = switch_api_port_get_internal(port_number);
    status = switch_pd_lag_group_table_delete_entry(
        device, port_info->lg_entry);
    status = switch_pd_ingress_port_mapping_table_delete_entry(
        device, port_info->hw_entry);
    return status;
}


switch_status_t
switch_api_port_print_entry(switch_port_t port)
{
    switch_port_info_t *port_info = NULL;

    port_info = &switch_port_info[port];
    printf("\n\nport number: %d", SWITCH_PORT_ID(port_info));
    printf("\n\tifindex: %x",  port_info->ifindex);
    printf("\n");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t
switch_api_port_print_all(void)
{
    switch_port_t port = 0;
    for (port = 0; port < SWITCH_API_MAX_PORTS; port++) {
        switch_api_port_print_entry(port);
    }
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t
switch_api_port_storm_control_set(
        switch_device_t device,
        switch_port_t port,
        switch_packet_type_t pkt_type,
        switch_handle_t meter_handle)
{
    switch_port_info_t *port_info = NULL;
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    port = handle_to_id(port);
    if (port > SWITCH_API_MAX_PORTS) {
        return SWITCH_STATUS_INVALID_PARAMETER;
    }

    port_info = &switch_port_info[port];
    port_info->meter_handle[pkt_type] = meter_handle;
    if (meter_handle) {
        status = switch_pd_storm_control_table_add_entry(
                             device,
                             port,
                             1000,
                             pkt_type,
                             handle_to_id(meter_handle),
                             &port_info->meter_pd_hdl[pkt_type]);
    } else {
        if (port_info->meter_pd_hdl) {
            status = switch_pd_storm_control_table_delete_entry(
                             device,
                             port_info->meter_pd_hdl[pkt_type]);
        }
    }
    return status;
}

switch_status_t
switch_api_port_storm_control_get(
        switch_device_t device,
        switch_port_t port,
        switch_packet_type_t pkt_type,
        switch_handle_t *meter_handle)
{
    switch_port_info_t *port_info = NULL;

    port = handle_to_id(port);
    if (port > SWITCH_API_MAX_PORTS) {
        return SWITCH_STATUS_INVALID_PARAMETER;
    }

    port_info = &switch_port_info[port];
    *meter_handle = port_info->meter_handle[pkt_type];
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t
switch_api_storm_control_stats_get(switch_device_t device,
                          switch_handle_t meter_handle,
                          uint8_t count,
                          switch_meter_stats_t *counter_ids,
                          switch_counter_t *counters)
{
    switch_status_t                    status = SWITCH_STATUS_SUCCESS;
    switch_meter_info_t               *meter_info = NULL;
    switch_meter_stats_info_t         *stats_info = NULL;
    int                                index = 0;
    switch_bd_stats_id_t               counter_id = 0;

    meter_info = switch_meter_info_get(meter_handle);
    if (!meter_info) {
        return SWITCH_STATUS_ITEM_NOT_FOUND;
    }

    stats_info = meter_info->stats_info;
    status = switch_pd_storm_control_stats_get(device, meter_info);
    for (index = 0; index < count; index++) {
        counter_id = counter_ids[index];
        counters[index] = stats_info->counters[counter_id];
    }
    return status;
}

bool
switch_port_is_cpu_port(switch_handle_t port_hdl)
{
    uint32_t port_id = handle_to_id(port_hdl);
    return port_id == CPU_PORT_ID;
}
