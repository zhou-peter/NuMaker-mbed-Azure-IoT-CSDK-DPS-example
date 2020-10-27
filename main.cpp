/*
 * Copyright (c) 2020, Nuvoton Technology Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>

#include "mbed.h"
#include "rtos/ThisThread.h"
#include "NTPClient.h"

#include "iothub.h"
#include "iothub_message.h"
#include "iothub_client_version.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/xlogging.h"

#include "iothub_device_client_ll.h"
#include "iothub_client_options.h"
#include "azure_prov_client/prov_device_ll_client.h"
#include "azure_prov_client/prov_security_factory.h"

#include "certs.h"

#define MBED_CONF_APP_HSM_TYPE                      HSM_TYPE_SYMM_KEY
#define MBED_CONF_APP_PROVISION_REGISTRATION_ID     "my-symm-device-001"
#define MBED_CONF_APP_PROVISION_SYMMETRIC_KEY       "hkNaMnuW1AohmWcYE4qNQipoEtlL3q297PoiVlG/wyJhTSVEqeUMOoQ78vRON0uOV9+c3BzxRY8ul2RWhDYM/g=="
#define MBED_CONF_APP_PROVISION_ENDPOINT            "global.azure-devices-provisioning.net"
#define MBED_CONF_APP_PROVISION_ID_SCOPE            "0ne001568DC"
#define MBED_CONF_APP_IOTHUB_CLIENT_TRACE   1

#define HSM_TYPE_TPM                        1
#define HSM_TYPE_X509                       2
#define HSM_TYPE_HTTP_EDGE                  3
#define HSM_TYPE_SYMM_KEY                   4

#define NET_PROTO_MQTT                      1
#define NET_PROTO_MQTT_OVER_WEBSOCKETS      2
#define NET_PROTO_AMQP                      3
#define NET_PROTO_AMQP_OVER_WEBSOCKETS      4
#define NET_PROTO_HTTP                      5

// MQTT protocol
#include "iothubtransportmqtt.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"

// Enable static CA Certificates defined in the SDK
#include "certs.h"

// This sample is to demonstrate iothub reconnection with provisioning and should not
// be confused as production code

MU_DEFINE_ENUM_STRINGS_WITHOUT_INVALID(PROV_DEVICE_RESULT, PROV_DEVICE_RESULT_VALUE);
MU_DEFINE_ENUM_STRINGS_WITHOUT_INVALID(PROV_DEVICE_REG_STATUS, PROV_DEVICE_REG_STATUS_VALUES);

static const char* global_prov_uri = MBED_CONF_APP_PROVISION_ENDPOINT;
static const char* id_scope = MBED_CONF_APP_PROVISION_ID_SCOPE;

#if (MBED_CONF_APP_HSM_TYPE == HSM_TYPE_SYMM_KEY)
static const char* registration_id = MBED_CONF_APP_PROVISION_REGISTRATION_ID;
static const char* symmetric_key = MBED_CONF_APP_PROVISION_SYMMETRIC_KEY;
#endif

#define PROXY_PORT                  8888
#define MESSAGES_TO_SEND            2
#define TIME_BETWEEN_MESSAGES       2
    
typedef struct CLIENT_SAMPLE_INFO_TAG
{
    unsigned int sleep_time;
    char* iothub_uri;
    char* access_key_name;
    char* device_key;
    char* device_id;
    int registration_complete;
} CLIENT_SAMPLE_INFO;

typedef struct IOTHUB_CLIENT_SAMPLE_INFO_TAG
{
    int connected;
    int stop_running;
} IOTHUB_CLIENT_SAMPLE_INFO;

static IOTHUBMESSAGE_DISPOSITION_RESULT receive_msg_callback(IOTHUB_MESSAGE_HANDLE message, void* user_context)
{
    (void)message;
    IOTHUB_CLIENT_SAMPLE_INFO* iothub_info = (IOTHUB_CLIENT_SAMPLE_INFO*)user_context;
    LogInfo("Stop message recieved from IoTHub\r\n");
    iothub_info->stop_running = 1;
    return IOTHUBMESSAGE_ACCEPTED;
}

static void registration_status_callback(PROV_DEVICE_REG_STATUS reg_status, void* user_context)
{
    (void)user_context;
    LogInfo("Provisioning Status: %s\r\n", MU_ENUM_TO_STRING(PROV_DEVICE_REG_STATUS, reg_status));
}

static void iothub_connection_status(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
    (void)reason;
    if (user_context == NULL)
    {
        LogInfo("iothub_connection_status user_context is NULL\r\n");
    }
    else
    {
        IOTHUB_CLIENT_SAMPLE_INFO* iothub_info = (IOTHUB_CLIENT_SAMPLE_INFO*)user_context;
        if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
        {
            iothub_info->connected = 1;
        }
        else
        {
            iothub_info->connected = 0;
            iothub_info->stop_running = 1;
        }
    }
}

static void register_device_callback(PROV_DEVICE_RESULT register_result, const char* iothub_uri, const char* device_id, void* user_context)
{
    if (user_context == NULL)
    {
        LogInfo("user_context is NULL\r\n");
    }
    else
    {
        CLIENT_SAMPLE_INFO* user_ctx = (CLIENT_SAMPLE_INFO*)user_context;
        if (register_result == PROV_DEVICE_RESULT_OK)
        {
            LogInfo("Registration Information received from service: %s!\r\n", iothub_uri);
            (void)mallocAndStrcpy_s(&user_ctx->iothub_uri, iothub_uri);
            (void)mallocAndStrcpy_s(&user_ctx->device_id, device_id);
            user_ctx->registration_complete = 1;
        }
        else
        {
            LogInfo("Failure encountered on registration %s\r\n", MU_ENUM_TO_STRING(PROV_DEVICE_RESULT, register_result) );
            user_ctx->registration_complete = 2;
        }
    }
}

// Global symbol referenced by the Azure SDK's port for Mbed OS, via "extern"
NetworkInterface *_defaultSystemNetwork;

int main()
{
#if (MBED_CONF_APP_HSM_TYPE == HSM_TYPE_TPM)
    SECURE_DEVICE_TYPE hsm_type = SECURE_DEVICE_TYPE_TPM;
#elif (MBED_CONF_APP_HSM_TYPE == HSM_TYPE_X509)
    SECURE_DEVICE_TYPE hsm_type = SECURE_DEVICE_TYPE_X509;
#elif (MBED_CONF_APP_HSM_TYPE == HSM_TYPE_HTTP_EDGE)
    SECURE_DEVICE_TYPE hsm_type = SECURE_DEVICE_TYPE_HTTP_EDGE;
#elif (MBED_CONF_APP_HSM_TYPE == HSM_TYPE_SYMM_KEY)
    SECURE_DEVICE_TYPE hsm_type = SECURE_DEVICE_TYPE_SYMMETRIC_KEY;
#endif

    LogInfo("Connecting to the network");

    _defaultSystemNetwork = NetworkInterface::get_default_instance();
    if (_defaultSystemNetwork == nullptr) {
        LogError("No network interface found");
        return -1;
    }

    int ret = _defaultSystemNetwork->connect();
    if (ret != 0) {
        LogError("Connection error: %d", ret);
        return -1;
    }
    LogInfo("Connection success, MAC: %s", _defaultSystemNetwork->get_mac_address());

    LogInfo("Getting time from the NTP server");

    NTPClient ntp(_defaultSystemNetwork);
    ntp.set_server("time.google.com", 123);
    time_t timestamp = ntp.get_timestamp();
    if (timestamp < 0) {
        LogError("Failed to get the current time, error: %ld", (long)timestamp);
        return -1;
    }
    LogInfo("Time: %s", ctime(&timestamp));

    rtc_init();
    rtc_write(timestamp);
    time_t rtc_timestamp = rtc_read(); // verify it's been successfully updated
    LogInfo("RTC reports %s", ctime(&rtc_timestamp));

    bool traceOn = MBED_CONF_APP_IOTHUB_CLIENT_TRACE;

    (void)IoTHub_Init();
    (void)prov_dev_security_init(hsm_type);

#if (MBED_CONF_APP_HSM_TYPE == HSM_TYPE_SYMM_KEY)
    // Set registration ID / symmetric key if using HSM symmkey
    prov_dev_set_symmetric_key_info(registration_id, symmetric_key);
#endif

    PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION prov_transport;
    CLIENT_SAMPLE_INFO user_ctx;

    memset(&user_ctx, 0, sizeof(CLIENT_SAMPLE_INFO));

    // Protocol to USE - MQTT
    prov_transport = Prov_Device_MQTT_Protocol;

    // Set ini
    user_ctx.registration_complete = 0;
    user_ctx.sleep_time = 10;

    LogInfo("Provisioning API Version: %s\r\n", Prov_Device_LL_GetVersionString());
    LogInfo("Iothub API Version: %s\r\n", IoTHubClient_GetVersionString());

    PROV_DEVICE_LL_HANDLE handle;
    if ((handle = Prov_Device_LL_Create(global_prov_uri, id_scope, prov_transport)) == NULL)
    {
        LogInfo("failed calling Prov_Device_LL_Create\r\n");
    }
    else
    {
        Prov_Device_LL_SetOption(handle, PROV_OPTION_LOG_TRACE, &traceOn);

        // Enable static CA Certificates defined in the SDK
        Prov_Device_LL_SetOption(handle, OPTION_TRUSTED_CERT, certificates);

        // This option sets the registration ID it overrides the registration ID that is 
        // set within the HSM so be cautious if setting this value
        //Prov_Device_LL_SetOption(handle, PROV_REGISTRATION_ID, "[REGISTRATION ID]");

        if (Prov_Device_LL_Register_Device(handle, register_device_callback, &user_ctx, registration_status_callback, &user_ctx) != PROV_DEVICE_RESULT_OK)
        {
            LogInfo("failed calling Prov_Device_LL_Register_Device\r\n");
        }
        else
        {
            do
            {
                Prov_Device_LL_DoWork(handle);
                ThreadAPI_Sleep(user_ctx.sleep_time);
            } while (user_ctx.registration_complete == 0);
        }
        Prov_Device_LL_Destroy(handle);
    }

    if (user_ctx.registration_complete != 1)
    {
        LogInfo("registration failed!\r\n");
    }
    else
    {
        IOTHUB_CLIENT_TRANSPORT_PROVIDER iothub_transport;

        // Protocol to USE - MQTT
        iothub_transport = MQTT_Protocol;

        IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;

        LogInfo("Creating IoTHub Device handle\r\n");
        if ((device_ll_handle = IoTHubDeviceClient_LL_CreateFromDeviceAuth(user_ctx.iothub_uri, user_ctx.device_id, iothub_transport) ) == NULL)
        {
            LogInfo("failed create IoTHub client from connection string %s!\r\n", user_ctx.iothub_uri);
        }
        else
        {
            IOTHUB_CLIENT_SAMPLE_INFO iothub_info;
            TICK_COUNTER_HANDLE tick_counter_handle = tickcounter_create();
            tickcounter_ms_t current_tick;
            tickcounter_ms_t last_send_time = 0;
            size_t msg_count = 0;
            iothub_info.stop_running = 0;
            iothub_info.connected = 0;

            (void)IoTHubDeviceClient_LL_SetConnectionStatusCallback(device_ll_handle, iothub_connection_status, &iothub_info);

            // Set any option that are neccessary.
            // For available options please see the iothub_sdk_options.md documentation

            IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_LOG_TRACE, &traceOn);

            // Enable static CA Certificates defined in the SDK
            IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_TRUSTED_CERT, certificates);

            (void)IoTHubDeviceClient_LL_SetMessageCallback(device_ll_handle, receive_msg_callback, &iothub_info);

            LogInfo("Sending 1 messages to IoTHub every %d seconds for %d messages (Send any message to stop)\r\n", TIME_BETWEEN_MESSAGES, MESSAGES_TO_SEND);
            do
            {
                if (iothub_info.connected != 0)
                {
                    // Send a message every TIME_BETWEEN_MESSAGES seconds
                    (void)tickcounter_get_current_ms(tick_counter_handle, &current_tick);
                    if ((current_tick - last_send_time) / 1000 > TIME_BETWEEN_MESSAGES)
                    {
                        static char msgText[1024];
                        sprintf_s(msgText, sizeof(msgText), "{ \"message_index\" : \"%zu\" }", msg_count++);

                        IOTHUB_MESSAGE_HANDLE msg_handle = IoTHubMessage_CreateFromByteArray((const unsigned char*)msgText, strlen(msgText));
                        if (msg_handle == NULL)
                        {
                            LogInfo("ERROR: iotHubMessageHandle is NULL!\r\n");
                        }
                        else
                        {
                            if (IoTHubDeviceClient_LL_SendEventAsync(device_ll_handle, msg_handle, NULL, NULL) != IOTHUB_CLIENT_OK)
                            {
                                LogInfo("ERROR: IoTHubClient_LL_SendEventAsync..........FAILED!\r\n");
                            }
                            else
                            {
                                (void)tickcounter_get_current_ms(tick_counter_handle, &last_send_time);
                                LogInfo("IoTHubClient_LL_SendEventAsync accepted message [%zu] for transmission to IoT Hub.\r\n", msg_count);

                            }
                            IoTHubMessage_Destroy(msg_handle);
                        }
                    }
                }
                IoTHubDeviceClient_LL_DoWork(device_ll_handle);
                ThreadAPI_Sleep(1);
            } while (iothub_info.stop_running == 0 && msg_count < MESSAGES_TO_SEND);

            size_t index = 0;
            for (index = 0; index < 10; index++)
            {
                IoTHubDeviceClient_LL_DoWork(device_ll_handle);
                ThreadAPI_Sleep(1);
            }
            tickcounter_destroy(tick_counter_handle);
            // Clean up the iothub sdk handle
            IoTHubDeviceClient_LL_Destroy(device_ll_handle);
        }
    }
    free(user_ctx.iothub_uri);
    free(user_ctx.device_id);
    prov_dev_security_deinit();

    // Free all the sdk subsystem
    IoTHub_Deinit();

    LogInfo("Press any enter to continue:\r\n");
    (void)getchar();

    return 0;
}
