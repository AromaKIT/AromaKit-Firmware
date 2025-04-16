#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack.h>
#include "bt_spp.h"

#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t rfcomm_channel_id;
static uint8_t  spp_service_buffer[150];
static btstack_packet_callback_registration_t hci_event_callback_registration;


static char *bdAddrToString(char *str, uint8_t bd_addr[6]) {
    // 00:00:00:00:00:00
    snprintf(str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
        bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);
    return str;
}


/* @section SPP Service Setup
 *s
 * @text To provide an SPP service, the L2CAP, RFCOMM, and SDP protocol layers
 * are required. After setting up an RFCOMM service with channel nubmer
 * RFCOMM_SERVER_CHANNEL, an SDP record is created and registered with the SDP server.
 * Example code for SPP service setup is
 * provided in Listing SPPSetup. The SDP record created by function
 * spp_create_sdp_record consists of a basic SPP definition that uses the provided
 * RFCOMM channel ID and service name. For more details, please have a look at it
 * in \path{src/sdp_util.c}.
 * The SDP record is created on the fly in RAM and is deterministic.
 * To preserve valuable RAM, the result could be stored as constant data inside the ROM.
 */
static void spp_service_setup(void) {

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    rfcomm_init();
    rfcomm_register_service(packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff);  // reserved channel, mtu limited by l2cap

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, sdp_create_service_record_handle(), RFCOMM_SERVER_CHANNEL, "SPP Counter");
    btstack_assert(de_get_len( spp_service_buffer) <= sizeof(spp_service_buffer));
    sdp_register_service(spp_service_buffer);
}

static btstack_timer_source_t heartbeat;
static char lineBuffer[30];
static void heartbeat_handler(struct btstack_timer_source *ts) {
    static int counter = 0;

    if (rfcomm_channel_id){
        snprintf(lineBuffer, sizeof(lineBuffer), "BTstack counter %04u\n", ++counter);
        printf("%s", lineBuffer);

        rfcomm_request_can_send_now_event(rfcomm_channel_id);
    }

    // btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    // btstack_run_loop_add_timer(ts);
}

static void one_shot_timer_setup(void){
    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);
}

/* @section Bluetooth Logic 
 * @text The Bluetooth logic is implemented within the 
 * packet handler, see Listing SppServerPacketHandler. In this example, 
 * the following events are passed sequentially: 
 * - BTSTACK_EVENT_STATE,
 * - HCI_EVENT_PIN_CODE_REQUEST (Standard pairing) or 
 * - HCI_EVENT_USER_CONFIRMATION_REQUEST (Secure Simple Pairing),
 * - RFCOMM_EVENT_INCOMING_CONNECTION,
 * - RFCOMM_EVENT_CHANNEL_OPENED, 
 * - RFCOMM_EVETN_CAN_SEND_NOW, and
 * - RFCOMM_EVENT_CHANNEL_CLOSED
 */

/* @text Upon receiving HCI_EVENT_PIN_CODE_REQUEST event, we need to handle
 * authentication. Here, we use a fixed PIN code "0000".
 *
 * When HCI_EVENT_USER_CONFIRMATION_REQUEST is received, the user will be 
 * asked to accept the pairing request. If the IO capability is set to 
 * SSP_IO_CAPABILITY_DISPLAY_YES_NO, the request will be automatically accepted.
 *
 * The RFCOMM_EVENT_INCOMING_CONNECTION event indicates an incoming connection.
 * Here, the connection is accepted. More logic is need, if you want to handle connections
 * from multiple clients. The incoming RFCOMM connection event contains the RFCOMM
 * channel number used during the SPP setup phase and the newly assigned RFCOMM
 * channel ID that is used by all BTstack commands and events.
 *
 * If RFCOMM_EVENT_CHANNEL_OPENED event returns status greater then 0,
 * then the channel establishment has failed (rare case, e.g., client crashes).
 * On successful connection, the RFCOMM channel ID and MTU for this
 * channel are made available to the heartbeat counter. After opening the RFCOMM channel, 
 * the communication between client and the application
 * takes place. In this example, the timer handler increases the real counter every
 * second. 
 *
 * RFCOMM_EVENT_CAN_SEND_NOW indicates that it's possible to send an RFCOMM packet
 * on the rfcomm_cid that is include

 */ 
static char send_buffer[128];
static int send_len = 0;
static char recv_line_buffer[128];
static int recv_size = 0;
static recv_callback_t recv_callback = NULL;
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    int i;
    hci_con_handle_t conn_handle;
    uint8_t bd_addr[6];
    char addr_str[18];

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_CONNECTION_COMPLETE:
                    conn_handle = hci_event_connection_complete_get_connection_handle(packet);
                    hci_event_connection_complete_get_bd_addr(packet, bd_addr);
                    printf("HCI connection complete: handle=0x%04x, addr=%s\n",
                        conn_handle, bdAddrToString(addr_str, bd_addr));
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    conn_handle = hci_event_disconnection_complete_get_connection_handle(packet);
                    printf("HCI disconnect complete: handle=0x%04x, reason 0x%02x\n",
                        conn_handle, hci_event_disconnection_complete_get_reason(packet));
                    break;

                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // ssp: inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection(rfcomm_channel_id);
                    break;

                case RFCOMM_EVENT_CHANNEL_OPENED:
                    if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM channel open failed, status 0x%02x\n", rfcomm_event_channel_opened_get_status(packet));
                    } else {
                        rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
                    }
                    break;
                case RFCOMM_EVENT_CAN_SEND_NOW:
                    if (send_len) {
                        rfcomm_send(rfcomm_channel_id, (uint8_t*) send_buffer, (uint16_t) send_len);
                        send_len = 0;
                    }
                    break;

                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM channel closed\n");
                    rfcomm_channel_id = 0;
                    break;

                default:
                    break;
            }
            break;

        case RFCOMM_DATA_PACKET:
            memcpy(recv_line_buffer, packet, size);
            recv_line_buffer[size] = 0;
            if (recv_callback) {
                recv_callback(recv_line_buffer, size);
            }
            // printf("RCV: '");
            // for (i=0;i<size;i++){
            //     putchar(packet[i]);
            // }
            // printf("'\n");
            break;

        default:
            break;
    }
}

size_t spp_write(void *buf, size_t size, size_t count) {
    memcpy(send_buffer, buf, (size*count));
    send_len = size*count;
    rfcomm_request_can_send_now_event(rfcomm_channel_id);
    return size*count;
}

void set_recv_callback(recv_callback_t func) {
    recv_callback = func;
}

void btstack_main() {
    one_shot_timer_setup();
    spp_service_setup();

    gap_discoverable_control(1);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_ONLY);
    gap_set_local_name("Pico 00:00:00:00:00:00");

    // turn on!
    hci_power_control(HCI_POWER_ON);
}
