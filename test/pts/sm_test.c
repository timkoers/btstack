/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

// *****************************************************************************
/* EXAMPLE_START(sm_test): Security Manager Test
 *
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack_config.h"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/le_device_db.h"
#include "ble/sm.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_stdin.h"
 
#define HEARTBEAT_PERIOD_MS 1000

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x06, 
    // Name
    0x0d, 0x09, 'S', 'M', ' ', 'P', 'e', 'r', 'i', 'p', 'h', 'e', 'a', 'l' 
};
const uint8_t adv_data_len = sizeof(adv_data);

// test profile
#include "sm_test.h"

static uint8_t sm_have_oob_data = 0;
static uint8_t sm_io_capabilities = 0;
static uint8_t sm_auth_req = 0;
static uint8_t sm_failure = 0;
// static uint8_t * sm_oob_data = (uint8_t *) "0123456789012345"; // = { 0x30...0x39, 0x30..0x35}
static uint8_t sm_oob_data[] = { 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  };

static int       we_are_central = 0;
static bd_addr_t peer_address;

static int ui_passkey = 0;
static int ui_digits_for_passkey = 0;

static btstack_timer_source_t heartbeat;
static uint8_t counter = 0;

static uint16_t connection_handle = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

typedef enum {
    TC_IDLE,
    TC_W4_SCAN_RESULT,
    TC_W4_CONNECT,
    TC_W4_SERVICE_RESULT,
    TC_W4_CHARACTERISTIC_RESULT,
    TC_W4_SUBSCRIBED,
    TC_SUBSCRIBED
} gc_state_t;

static gc_state_t state = TC_IDLE;

static uint8_t le_counter_service_uuid[16]        = { 0x00, 0x00, 0xFF, 0x10, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
static uint8_t le_counter_characteristic_uuid[16] = { 0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};

static gatt_client_service_t le_counter_service;
static gatt_client_characteristic_t le_counter_characteristic;

static gatt_client_notification_t notification_listener;
static void  heartbeat_handler(struct btstack_timer_source *ts){
    // restart timer
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
    counter++;
} 

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(buffer);
    printf("READ Callback, handle %04x, offset %u, buffer size %u\n", attribute_handle, offset, buffer_size);
    switch (attribute_handle){
        default:
            break;
    }
    return 0;
}

// write requests
static int att_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    printf("WRITE Callback, handle %04x, mode %u, offset %u, data: ", attribute_handle, transaction_mode, offset);
    printf_hexdump(buffer, buffer_size);

    switch (attribute_handle){
        case ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
            // short cut, send right away
            att_server_request_can_send_now_event(con_handle);
            break;
        default:
            break;
    }
    return 0;
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    int status;
    char message[30];

    switch(state){
        case TC_W4_SERVICE_RESULT:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    gatt_event_service_query_result_get_service(packet, &le_counter_service);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    if (packet[4] != 0){
                        printf("SERVICE_QUERY_RESULT - Error status %x.\n", packet[4]);
                        gap_disconnect(connection_handle);
                        break;  
                    } 
                    state = TC_W4_CHARACTERISTIC_RESULT;
                    printf("Search for counter characteristic.\n");
                    gatt_client_discover_characteristics_for_service_by_uuid128(handle_gatt_client_event, connection_handle, &le_counter_service, le_counter_characteristic_uuid);
                    break;
                default:
                    break;
            }
            break;
            
        case TC_W4_CHARACTERISTIC_RESULT:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    gatt_event_characteristic_query_result_get_characteristic(packet, &le_counter_characteristic);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    if (packet[4] != 0){
                        printf("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", packet[4]);
                        gap_disconnect(connection_handle);
                        break;  
                    } 
                    state = TC_W4_SUBSCRIBED;
                    printf("Configure counter for notify.\n");
                    status = gatt_client_write_client_characteristic_configuration(handle_gatt_client_event, connection_handle, &le_counter_characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    break;
                default:
                    break;
            }
            break;
        case TC_W4_SUBSCRIBED:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_QUERY_COMPLETE:
                    // register handler for notifications
                    state = TC_SUBSCRIBED;
                    printf("Subscribed, start listening\n");
                    gatt_client_listen_for_characteristic_value_updates(&notification_listener, handle_gatt_client_event, connection_handle, &le_counter_characteristic);
                    break;
                default:
                    break;
            }
            break;

        case TC_SUBSCRIBED:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_NOTIFICATION:
                    memset(message, 0, sizeof(message));
                    memcpy(message, gatt_event_notification_get_value(packet), gatt_event_notification_get_value_length(packet));
                    printf("COUNTER: %s\n", message);
                    log_info("COUNTER: %s", message);
                    break;
                default:
                    break;
            }

        default:
            break;
    }
    fflush(stdout);
}

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t local_addr;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        gap_local_bd_addr(local_addr);
                        printf("BD_ADDR: %s\n", bd_addr_to_str(local_addr));
                        // start connecting if we're central
                        if (we_are_central){
                            printf("CENTRAL: connect to %s\n", bd_addr_to_str(peer_address));
                            gap_connect(peer_address, BD_ADDR_TYPE_LE_PUBLIC);
                        }
                    }
                    break;
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            connection_handle = little_endian_read_16(packet, 4);
                            printf("CONNECTED: Connection handle 0x%04x\n", connection_handle);
                            if (we_are_central){
                                printf("REQUEST_PAIRING\n");
                                sm_request_pairing(connection_handle);
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    break;
                case SM_EVENT_JUST_WORKS_REQUEST:
                    printf("JUST_WORKS_REQUEST\n");
                    // sm_just_works_confirm(little_endian_read_16(packet, 2));
                    break;
                case SM_EVENT_PASSKEY_INPUT_NUMBER:
                    // display number
                    printf("PASSKEY_INPUT_NUMBER\n");
                    ui_passkey = 0;
                    ui_digits_for_passkey = 6;
                    // sm_keypress_notification(connection_handle, SM_KEYPRESS_PASSKEY_ENTRY_STARTED);
                    break;
                case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
                    // display number
                    printf("PASSKEY_DISPLAY_NUMBER: %06u\n", little_endian_read_32(packet, 11));
                    break;
                case SM_EVENT_PASSKEY_DISPLAY_CANCEL: 
                    break;
                case SM_EVENT_AUTHORIZATION_REQUEST:
                    break;
                case SM_EVENT_PAIRING_COMPLETE:
                    printf("PAIRING_COMPLETE: %u,%u\n", sm_event_pairing_complete_get_status(packet), sm_event_pairing_complete_get_reason(packet));
                    if (sm_event_pairing_complete_get_status(packet)) break;
                    if (we_are_central){
                        printf("Search for LE Counter service.\n");
                        state = TC_W4_SERVICE_RESULT;
                        gatt_client_discover_primary_services_by_uuid128(handle_gatt_client_event, connection_handle, le_counter_service_uuid);
                    }
                    break;
                case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
                    break;
                case ATT_EVENT_CAN_SEND_NOW:
                    att_server_notify(connection_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t *) "Pairing Success!", 16);
                    break;
                default:
                    break;
            }
    }
    fflush(stdout);
}

#define KEYPRESS_DELAY 100
static btstack_timer_source_t passkey_timer;
static int num_keypresses_to_send;

static void passkey_entry_complete(btstack_timer_source_t * ts){
    if (num_keypresses_to_send > 0){
        num_keypresses_to_send--;
        // sm_keypress_notification(connection_handle, SM_KEYPRESS_PASSKEY_DIGIT_ENTERED);
        btstack_run_loop_set_timer(ts, KEYPRESS_DELAY);
        btstack_run_loop_add_timer(ts);
        return;
    }
    if (num_keypresses_to_send == 0){
        num_keypresses_to_send--;
        // sm_keypress_notification(connection_handle, SM_KEYPRESS_PASSKEY_ENTRY_COMPLETED);
        btstack_run_loop_set_timer(ts, KEYPRESS_DELAY);
        btstack_run_loop_add_timer(ts);
        return;
    }
    printf("\nSending Passkey '%06u'\n", ui_passkey);
    if (sm_failure == SM_REASON_PASSKEY_ENTRY_FAILED){
        // simulate passkey failure
        ui_passkey--;        
    }
    sm_passkey_input(connection_handle, ui_passkey);
}

static void stdin_process(char c){
    // passkey input
    if (ui_digits_for_passkey){
        if (c < '0' || c > '9') return;
        printf("%c", c);
        fflush(stdout);
        ui_passkey = ui_passkey * 10 + c - '0';
        ui_digits_for_passkey--;
        if (ui_digits_for_passkey == 0){
            num_keypresses_to_send = 6;
            // set one-shot timer
            passkey_timer.process = &passkey_entry_complete;
            btstack_run_loop_set_timer(&passkey_timer, KEYPRESS_DELAY);
            btstack_run_loop_add_timer(&passkey_timer);
        }
        return;
    }

    switch (c){
        case 'a': // accept just works
            printf("accepting just works\n");
            sm_just_works_confirm(connection_handle);
            break;
        case 'd':
            printf("decline bonding\n");
            sm_bonding_decline(connection_handle);
            break;
        case 'x':
            printf("Exit\n");
            exit(0);
            break;
        default:
            break;
    }
    fflush(stdout);
    return;
}

static int get_oob_data_callback(uint8_t address_type, bd_addr_t addr, uint8_t * oob_data){
    UNUSED(address_type);
    log_info("get_oob_data_callback for %s", bd_addr_to_str(addr));
    (void)addr;
    if(!sm_have_oob_data) return 0;
    memcpy(oob_data, sm_oob_data, 16);
    return 1;
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    int arg = 1;
    
    while (arg < argc) {
        if(!strcmp(argv[arg], "-a") || !strcmp(argv[arg], "--address")){
            arg++;
            we_are_central = sscanf_bd_addr(argv[arg], peer_address);
            arg++;
        }
        if(!strcmp(argv[arg], "-i") || !strcmp(argv[arg], "--iocap")){
            arg++;
            sm_io_capabilities = atoi(argv[arg++]);
        }
        if(!strcmp(argv[arg], "-r") || !strcmp(argv[arg], "--authreq")){
            arg++;
            sm_auth_req = atoi(argv[arg++]);
        }
        if(!strcmp(argv[arg], "-f") || !strcmp(argv[arg], "--failure")){
            arg++;
            sm_failure = atoi(argv[arg++]);
        }
        if(!strcmp(argv[arg], "-o") || !strcmp(argv[arg], "--oob")){
            arg++;
            sm_have_oob_data = atoi(argv[arg++]);
        }
    }

    // parse command line flags

    printf("Security Managet Tester starting up...\n");
    log_info("IO_CAPABILITIES: %u", sm_io_capabilities);
    log_info("AUTH_REQ: %u", sm_auth_req);
    log_info("HAVE_OOB: %u", sm_have_oob_data);
    log_info("FAILURE: %u", sm_failure);
    if (we_are_central){
        log_info("ROLE: CENTRAL");
    } else {
        log_info("ROLE: PERIPHERAL");

        // setup advertisements
        uint16_t adv_int_min = 0x0030;
        uint16_t adv_int_max = 0x0030;
        uint8_t adv_type = 0;
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
        gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
        gap_advertisements_enable(1);
    }

    // inform about BTstack state
    hci_event_callback_registration.callback = &app_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // set up l2cap_le
    l2cap_init();
    
    // setup le device db
    le_device_db_init();

    // 
    gatt_client_init();

    // setup SM io capabilities & auth req
    sm_init();
    sm_set_io_capabilities(sm_io_capabilities);
    sm_set_authentication_requirements(sm_auth_req); 
    sm_register_oob_data_callback(get_oob_data_callback);

    if (sm_failure < SM_REASON_NUMERIC_COMPARISON_FAILED && sm_failure != SM_REASON_PASSKEY_ENTRY_FAILED){
        sm_test_set_pairing_failure(sm_failure);
    }

    sm_event_callback_registration.callback = &app_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    att_server_register_packet_handler(app_packet_handler);

    btstack_stdin_setup(stdin_process);

    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}

/* EXAMPLE_END */
