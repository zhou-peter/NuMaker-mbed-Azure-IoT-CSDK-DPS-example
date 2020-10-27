# Example for provisioning with Azure DPS on Nuvoton's Mbed Enabled boards

This is an example to show provisioning with [Azure IoT Hub Device Provisioning Service](https://docs.microsoft.com/en-us/azure/iot-dps/) on Nuvoton's Mbed Enabled boards.
It relies on the following modules:

-   [Mbed OS](https://github.com/ARMmbed/mbed-os)
-   [Azure IoT Device SDK port for Mbed OS](https://github.com/ARMmbed/mbed-client-for-azure):
    -   [Azure IoT C SDKs and Libraries](https://github.com/Azure/azure-iot-sdk-c)
    -   [Adapters for Mbed OS](https://github.com/ARMmbed/mbed-client-for-azure/tree/master/mbed/adapters)
    -   Other dependency libraries
-   [NTP client library](https://github.com/ARMmbed/ntp-client)

## Support targets

Platform                        |  Connectivity     | Notes
--------------------------------|-------------------|---------------
Nuvoton NUMAKER_PFM_NUC472      | Ethernet          |
Nuvoton NUMAKER_PFM_M487        | Ethernet          |
Nuvoton NUMAKER_IOT_M487        | Wi-Fi ESP8266     |
Nuvoton NUMAKER_IOT_M263A       | Wi-Fi ESP8266     |

## Support development tools

-   [Arm's Mbed Studio](https://os.mbed.com/docs/mbed-os/v6.3/build-tools/mbed-studio.html)
-   [Arm's Mbed CLI](https://os.mbed.com/docs/mbed-os/v6.3/build-tools/mbed-cli.html)

## Developer guide

This section is intended for developers to get started, import the example application, compile with Mbed CLI, and get it running and provisiong with Azure DPS.

### Hardware requirements

-   Nuvoton's Mbed Enabled board

### Software requirements

-   [Arm's Mbed CLI](https://os.mbed.com/docs/mbed-os/v6.3/build-tools/mbed-cli.html)
-   [Azure account](https://portal.azure.com/)

### Hardware setup

Connect target board to host through USB.

### Operations on Azure portal

Follow the [doc](https://docs.microsoft.com/en-us/azure/iot-dps/quick-setup-auto-provision) to set up DPS on Azure portal.

For easy, choose [individual enrollment](https://docs.microsoft.com/en-us/azure/iot-dps/concepts-service#individual-enrollment) using [symmetric key](https://docs.microsoft.com/en-us/azure/iot-dps/concepts-service#attestation-mechanism).
Take note of the following items.

-   [Device provisioning endpoint](https://docs.microsoft.com/en-us/azure/iot-dps/concepts-service#device-provisioning-endpoint):
    Service endpoint or global device endpoint from Provisioning service overview page
    
-   [ID scope](https://docs.microsoft.com/en-us/azure/iot-dps/concepts-service#id-scope):
    ID Scope value from Provisioning service overview page

-   [Registration ID](https://docs.microsoft.com/en-us/azure/iot-dps/concepts-service#registration-id):
    Registration ID provided when doing individual registration
    
-   [Symmetric key](https://docs.microsoft.com/en-us/azure/iot-dps/concepts-service#attestation-mechanism):
    Symmetric key from individual registration detail page

### Compile with Mbed CLI

In the following, we take [NuMaker-IoT-M487](https://os.mbed.com/platforms/NUMAKER-IOT-M487/) as example board to show this example.

1.  Clone the example and navigate into it
    ```sh
    $ git clone https://github.com/OpenNuvoton/NuMaker-mbed-Azure-IoT-CSDK-DPS-example
    $ cd NuMaker-mbed-Azure-IoT-CSDK-DPS-example
    ```

1.  Deploy necessary libraries
    ```sh
    $ mbed deploy
    ```

1.  Configure HSM type. Set `hsm_type` to `HSM_TYPE_SYMM_KEY` to match [symmetric key](https://docs.microsoft.com/en-us/azure/iot-dps/concepts-service#attestation-mechanism) attestation type.
    In `mbed_app.json`:
    ```json
        "hsm_type": {
            "help": "Select support HSM type",
            "options": ["HSM_TYPE_TPM", "HSM_TYPE_X509", "HSM_TYPE_HTTP_EDGE", "HSM_TYPE_SYMM_KEY"],
            "value": "HSM_TYPE_SYMM_KEY"
        },
    ```

1.  Configure DPS parameters. They should have been noted in [above](#operations-on-azure-portal).
    In `mbed_app.json`:
    ```json
        "provision_registration_id": {
            "help": "Registration ID when HSM_TYPE_SYMM_KEY is supported; Ignored for other HSM types",
            "value": "\"REGISTRATION_ID\""
        },
        "provision_symmetric_key": {
            "help": "Symmetric key when HSM_TYPE_SYMM_KEY is supported; Ignored for other HSM types",
            "value": "\"SYMMETRIC_KEY\""
        },
        "provision_endpoint": {
            "help": "Device provisioning service URI",
            "value": "\"global.azure-devices-provisioning.net\""
        },
        "provision_id_scope": {
            "help": "Device provisioning service ID scope",
            "value": "\"ID_SCOPE\""
        },
    ```

    **NOTE**: For non-symmetric key attestation type, `provision_symmetric_key` is unnecessary and `provision_registration_id` is acquired through other means.

1.  Eenable Azure C-SDK provisioning client module and custom HSM.
    In `mbed_app.json`:
    ```
    "macros": [
        "USE_PROV_MODULE",
        "HSM_AUTH_TYPE_CUSTOM"
    ],
    ```

1.  Configure network interface
    -   Ethernet: Need no further configuration.
    -   WiFi: In `mbed_app.json`, configure WiFi `SSID`/`PASSWORD`.
        ```json
            "nsapi.default-wifi-ssid"               : "\"SSID\"",
            "nsapi.default-wifi-password"           : "\"PASSWORD\"",
        ```

1.  Build the example on **NUMAKER_IOT_M487** target and **ARM** toolchain
    ```sh
    $ mbed compile -m NUMAKER_IOT_M487 -t ARM
    ```

1.  Flash by drag-n-drop'ing the built image file below onto **NuMaker-IoT-M487** board

    `BUILD/NUMAKER_IOT_M487/ARM/NuMaker-mbed-Azure-IoT-CSDK-DPS-example.bin`

### Monitor the application through host console

Configure host terminal program with **115200/8-N-1**, and you should see log similar to below:

```
Info: Connecting to the network
Info: Connection success, MAC: a4:cf:12:b7:82:3b
Info: Getting time from the NTP server
Info: Time: Tue Oct27 3:13:29 2020

Info: RTC reports Tue Oct27 3:13:29 2020

Info: Provisioning API Version: 1.3.9

Info: Iothub API Version: 1.3.9

Info: Provisioning Status: PROV_DEVICE_REG_STATUS_CONNECTED

Info: Provisioning Status: PROV_DEVICE_REG_STATUS_ASSIGNING

Info: Registration Information received from service: nuvoton-test-001.azure-devices.net!

Info: Creating IoTHub Device handle

Info: Sending 1 messages to IoTHub every 2 seconds for 2 messages (Send any message to stop)

Info: IoTHubClient_LL_SendEventAsync accepted message [1] for transmission to IoT Hub.

Info: IoTHubClient_LL_SendEventAsync accepted message [2] for transmission to IoT Hub.

Info: Press any enter to continue:

```

### Walk through source code

#### Custom HSM (`hsm_custom/`)

[Azure C-SDK Provisioning Client](https://github.com/Azure/azure-iot-sdk-c/blob/master/provisioning_client/devdoc/using_provisioning_client.md) requires [HSM](https://docs.microsoft.com/en-us/azure/iot-dps/concepts-service#hardware-security-module).
This directory provides one custom HSM library for development.
It is adapted from [Azure C-SDK custom hsm example](https://github.com/Azure/azure-iot-sdk-c/tree/master/provisioning_client/samples/custom_hsm_example) and is a trivial implementation.
**DO NOT** use it for production.

##### Using DPS with symmetric key

If you run provisioning process like this example, provide `provision_registration_id` and `provision_symmetric_key` as [above](#compile-with-mbed-cli).
During provisioning process, `SYMMETRIC_KEY` and `REGISTRATION_NAME` will be overridden through `custom_hsm_set_key_info`.
So you needn't override `SYMMETRIC_KEY` and `REGISTRATION_NAME` below.

If you don't run provisioning process and connect straight to IoT Hub instead, override `SYMMETRIC_KEY` and `REGISTRATION_NAME` below.

```C
// Provided for sample only
static const char* const SYMMETRIC_KEY = "Symmetric Key value";
static const char* const REGISTRATION_NAME = "Registration Name";
```

##### Using DPS with X.509 certificate

First, use the same [step](https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-security-x509-get-started) to set up X.509 security on Azure portal.

To use DPS with X.509 certificate, override `COMMON_NAME`, `CERTIFICATE`, and `PRIVATE_KEY` below.
`COMMON_NAME` is the registration ID.
`CERTIFICATE` and `PRIVATE_KEY` are your self-signed or CA-signed certificate and private key in PEM format.

```C
// This sample is provided for sample only.  Please do not use this in production
// For more information please see the devdoc using_custom_hsm.md
static const char* const COMMON_NAME = "custom-hsm-example";
static const char* const CERTIFICATE = "-----BEGIN CERTIFICATE-----""\n"
"BASE64 Encoded certificate Here""\n"
"-----END CERTIFICATE-----";
static const char* const PRIVATE_KEY = "-----BEGIN PRIVATE KEY-----""\n"
"BASE64 Encoded certificate Here""\n"
"-----END PRIVATE KEY-----";
```

#### Platform entropy source (`targets/TARGET_NUVOTON/platform_entropy.cpp`)

Mbedtls requires [entropy source](https://os.mbed.com/docs/mbed-os/v6.4/porting/entropy-sources.html).
On targets with `TRNG` hardware, Mbed OS has supported it.
On targets without `TRNG` hardware, substitute platform entropy source must be provided.
This directory provides one platform entropy source implementation for Nuvoton's targets without `TRNG` hardware.

## Known issues or limitations

1.  Only symmetric key/X.509 certificate attestation types are verified. Other attestation types are not supported.
1.  The attached custom HSM library is one trivial implementation. **DO NOT** use it for production.
