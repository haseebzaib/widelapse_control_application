# Widelapse MQTT integration

We need an MQTT broker and a backend consumer so devices can register and send network status and system diagnostics. The flow below matches the current device implementation.

Please share the broker hostname, port, and any authentication or TLS requirements so we can configure the devices. The device currently uses a public broker for testing.

## Connection and registration

Each device connects using a client ID such as `widelapse_aabbccddeeff`. This comes from its Ethernet MAC address with the colons removed. The same value is sent as `deviceId` during registration.

After connecting, the device subscribes to `widelapse/registration/1` and publishes this JSON to `widelapse/registration`:

```json
{
  "deviceId": "widelapse_aabbccddeeff",
  "deviceType": "gateway"
}
```

The backend should create or find the device record, then publish this response to `widelapse/registration/1`:

```json
{
  "deviceId": "widelapse_aabbccddeeff",
  "resp": "ack"
}
```

The response topic is shared. Each device checks `deviceId` and only accepts an `ack` for its own ID.

The device retries registration every 10 seconds until it receives that acknowledgement. Registration also runs again when the application restarts, so repeated requests for the same ID should return an acknowledgement without creating duplicate records.

If registration cannot be accepted, the backend can respond with `"resp": "nack"` and an optional `reason`. The current device keeps retrying; it does not act on the reason.

## Regular updates

Once registration is acknowledged, the device sends two separate JSON messages approximately every 60 seconds:

| Data | Topic |
| --- | --- |
| Network status | `widelapse/network/<clientId>` |
| System diagnostics | `widelapse/diagnostic/<clientId>` |

The device ID comes from the topic; these payloads do not contain a `deviceId` field. The backend can subscribe to `widelapse/network/+` and `widelapse/diagnostic/+` to receive updates from all devices.

Device publications and its registration-response subscription use QoS 1. Messages are not retained. Please handle duplicate deliveries and record the server receipt time for each update.

### Network status example

Topic: `widelapse/network/widelapse_aabbccddeeff`

```json
{
  "links": {
    "cellular": {
      "addresses": [],
      "connected": false,
      "enabled": true,
      "healthy": false,
      "interface": null,
      "internet": false,
      "present": false,
      "rssi_dbm": null,
      "signal_percent": null
    },
    "eth": {
      "activation": "/org/freedesktop/NetworkManager/ActiveConnection/5",
      "addresses": ["10.42.0.35"],
      "connected": true,
      "control_interface": "end0",
      "enabled": true,
      "healthy": true,
      "interface": "end0",
      "internet": true,
      "present": true,
      "rssi_dbm": null,
      "signal_percent": null,
      "state": 100,
      "uuid": "dcc0f047-d026-504a-8abf-5ae97c581bd2"
    },
    "wifi": {
      "activation": null,
      "addresses": [],
      "connected": false,
      "control_interface": "wlan0",
      "enabled": true,
      "healthy": false,
      "interface": "wlan0",
      "internet": false,
      "present": true,
      "rssi_dbm": null,
      "signal_percent": null,
      "state": 30,
      "uuid": null
    }
  },
  "modem": {
    "present": false,
    "state": "absent"
  },
  "routing_applied": true,
  "selected": "eth",
  "updated_at": 1794329639
}
```

`selected` identifies the selected connection. Each entry in `links` reports that interface's status, addresses, and signal information where available. `updated_at` is a Unix timestamp in seconds. Some fields may be missing or `null` when unavailable, so the backend should accept that.

### System diagnostics example

Topic: `widelapse/diagnostic/widelapse_aabbccddeeff`

```json
{
  "cpu": {
    "temp0": 47,
    "temp1": 46,
    "temp2": 46,
    "temp3": 47
  },
  "ram": {
    "free": 1593,
    "total": 1973
  },
  "storage": {
    "application": {
      "free": 23553,
      "total": 23613,
      "used": 43
    },
    "rootfs": {
      "free": 1089,
      "total": 2950,
      "used": 1783
    }
  }
}
```

CPU temperatures are in degrees Celsius. RAM and storage values are in MiB. `rootfs` reports `/`, and `application` reports the filesystem containing `/opt/widelapse`. Storage `free` is the space available to an unprivileged user, so `used + free` may not equal `total`.

## Remote commands

The cloud can send Wi-Fi settings, SFTP settings, and capture/transfer schedules, scan nearby Wi-Fi networks, and request capture/upload counts from a registered device. All commands use the same topics:

| Direction | Topic |
| --- | --- |
| Cloud → device: command | `widelapse/remoteCommands/0/<clientId>` |
| Device → cloud: result | `widelapse/remoteCommands/1/<clientId>` |

For example, publish commands for `widelapse_aabbccddeeff` to `widelapse/remoteCommands/0/widelapse_aabbccddeeff` and listen for results on `widelapse/remoteCommands/1/widelapse_aabbccddeeff`. The backend can subscribe to `widelapse/remoteCommands/1/+` for all devices.

Subscribe for results before sending a command. Publish with QoS 1 and retain disabled so old commands are not replayed when a device connects. The device subscribes and replies at QoS 1, with replies not retained. Commands are processed after registration is acknowledged.

Requests contain `cmdType` and `cmdData`. Replies normally return the same `cmdType`, with `status` and `reason` inside `cmdData`. The current exception is `get_sftp_information`, which replies with `cmdType: "sftp_information"`. Queries also return a `data` field on success. Command names are case-sensitive:

| `cmdType` | Purpose |
| --- | --- |
| `wifiSetup` | Update Wi-Fi credentials |
| `scheduling_setup` | Replace capture and transfer schedules |
| `wifi_scan` | Scan nearby Wi-Fi access points |
| `total_shots` | Get successful and failed capture counts |
| `picture_uploads` | Get local image and upload counts |
| `sftp_information` | Update the SFTP server settings |
| `get_sftp_information` | Read the current SFTP server settings |

There is no request ID in the current format. Send one command at a time per device and wait for its result before sending the next. A broker delivery acknowledgement does not confirm that a command was applied; use the device's result message. If no result arrives, keep the outcome as unknown. Unknown command names currently receive no reply.

### Wi-Fi setup

Send this payload on the command topic:

```json
{
  "cmdType": "wifiSetup",
  "cmdData": {
    "ssid": "Site-WiFi",
    "password": "example-password"
  }
}
```

Both `ssid` and `password` are required strings. The device replaces the first saved Wi-Fi network's credentials and requests a network reload.

Success reply on the result topic:

```json
{
  "cmdType": "wifiSetup",
  "cmdData": {
    "status": "success",
    "reason": ""
  }
}
```

Failure reply:

```json
{
  "cmdType": "wifiSetup",
  "cmdData": {
    "status": "failure",
    "reason": "Unable to update WiFi configuration"
  }
}
```

A success response confirms that the settings were saved. It does not confirm that Wi-Fi connected or has internet access; the current handler does not check the reload result. Use subsequent network updates to check `links.wifi.connected` and `links.wifi.internet`. Reloading the network may interrupt MQTT, so the reply may not reach the cloud immediately.

### Scheduling setup

This command replaces the complete schedule for both camera capture and SFTP transfers. Always send every profile that should remain active, including profiles that have not changed. If no `transfer` profiles are included, existing transfer profiles are removed; the same applies to `camera` profiles.

Example: capture every 60 seconds and transfer pending images every 300 seconds, Monday to Friday, from 08:00 up to 18:00 in the device's local timezone.

```json
{
  "cmdType": "scheduling_setup",
  "cmdData": {
    "schedule": [
      {
        "type": "camera",
        "weekDays": [1, 2, 3, 4, 5],
        "startTime": 480,
        "endTime": 1080,
        "interval": 60
      },
      {
        "type": "transfer",
        "weekDays": [1, 2, 3, 4, 5],
        "startTime": 480,
        "endTime": 1080,
        "interval": 300
      }
    ]
  }
}
```

Each profile requires these fields:

| Field | Value |
| --- | --- |
| `type` | `"camera"` for image capture or `"transfer"` for SFTP transfer |
| `weekDays` | Array of integers: Sunday = `0`, Monday = `1`, through Saturday = `6`. Use `[-1]` or `[]` for every day. Do not combine `-1` with other days. |
| `startTime` | Integer minutes since midnight, `0`–`1439`, inclusive |
| `endTime` | Integer minutes since midnight, `1`–`1440`, exclusive |
| `interval` | Positive integer seconds, up to `2147483647` |

Use `weekDays` exactly as written, including the capital `D`. Times use the device's local timezone. `startTime` must be less than `endTime`; `0` to `1440` covers a full day. Split overnight windows into two profiles, assigning the second profile to the following day where needed.

The scheduler runs a profile when it first becomes active, then repeats at its interval while the window remains active. A newly applied profile can therefore run on the next scheduler check if the current time falls inside its window.

The device validates the complete list before saving it and replacing the active profiles. The saved schedule is loaded again when the application starts. Invalid input is rejected before changing the existing schedule.

Success reply:

```json
{
  "cmdType": "scheduling_setup",
  "cmdData": {
    "status": "success",
    "reason": ""
  }
}
```

Failure reply example:

```json
{
  "cmdType": "scheduling_setup",
  "cmdData": {
    "status": "failure",
    "reason": "startTime must be less than endTime"
  }
}
```

Failure reasons vary; display or log the returned `reason`. Success confirms that the schedule was saved and applied, rather than confirming that a capture or transfer completed.

To clear all camera and transfer schedules, send:

```json
{
  "cmdType": "scheduling_setup",
  "cmdData": {
    "schedule": []
  }
}
```

### Wi-Fi scan

Send on `widelapse/remoteCommands/0/<clientId>`:

```json
{
  "cmdType": "wifi_scan",
  "cmdData": {}
}
```

The device scans nearby access points and replies on `widelapse/remoteCommands/1/<clientId>`. This does not change credentials or connect to a network.

Example success reply:

```json
{
  "cmdType": "wifi_scan",
  "cmdData": {
    "status": "success",
    "data": {
      "ok": true,
      "interface": "wlan0",
      "updated_at": 1795447294,
      "networks": [
        {
          "ssid": "HUAWEI-FxmS",
          "ssid_hex": "4855415745492d46786d53",
          "hidden": false,
          "bssid": "1C:3D:2F:63:AD:8C",
          "signal_percent": 24,
          "frequency_mhz": 2442,
          "security": "WPA/RSN",
          "wpa_flags": 332,
          "rsn_flags": 332,
          "connected": false
        }
      ]
    },
    "reason": ""
  }
}
```

`networks` is sorted by signal strength, strongest first. An empty array is a valid result. Each entry describes an access point, so several entries can have the same SSID and different BSSIDs.

| Field | Meaning |
| --- | --- |
| `ssid` / `ssid_hex` | Network name for display / raw SSID bytes encoded as hex |
| `hidden` | Whether the access point has an empty SSID |
| `bssid` | Access point MAC address |
| `signal_percent` | Signal strength as a percentage |
| `frequency_mhz` | Radio frequency in MHz |
| `security` | `OPEN`, `WEP`, `WPA`, `RSN`, or `WPA/RSN` |
| `wpa_flags` / `rsn_flags` | Numeric security capability flags from NetworkManager |
| `connected` | Whether this is the currently connected access point |

`updated_at` is a Unix timestamp in seconds. The scan can take around 15 seconds, in addition to command processing time; allow for that when waiting for a reply. Wi-Fi must already be enabled and available.

If the scan script fails, the current device returns a failure response such as:

```json
{
  "cmdType": "wifi_scan",
  "cmdData": {
    "status": "failure",
    "reason": "failed to close network script"
  }
}
```

That is the current script-exit error text; it does not necessarily mean closing the script was the underlying problem. The reply does not include `data` on failure.

### Capture counts

Send on `widelapse/remoteCommands/0/<clientId>`:

```json
{
  "cmdType": "total_shots",
  "cmdData": {}
}
```

Example reply on `widelapse/remoteCommands/1/<clientId>`:

```json
{
  "cmdType": "total_shots",
  "cmdData": {
    "status": "success",
    "data": {
      "totalShotsTaken": 120,
      "totalShotsFailed": 3
    },
    "reason": ""
  }
}
```

`totalShotsTaken` counts successful captures and `totalShotsFailed` counts failed capture attempts. These counters cover the current application run and reset when the application restarts. They are not the camera's lifetime shutter count. This command only reads the counters; it does not take a picture.

### Picture upload counts

Send on `widelapse/remoteCommands/0/<clientId>`:

```json
{
  "cmdType": "picture_uploads",
  "cmdData": {}
}
```

Example reply on `widelapse/remoteCommands/1/<clientId>`:

```json
{
  "cmdType": "picture_uploads",
  "cmdData": {
    "status": "success",
    "data": {
      "totalImagesTaken": 120,
      "totalImagesTransfered": 100,
      "totalImagesFailed": 2
    },
    "reason": ""
  }
}
```

| Field | Meaning |
| --- | --- |
| `totalImagesTaken` | Number of `.jpg` files currently in the device's image directory, including uploaded images still stored there |
| `totalImagesTransfered` | Number of those images with a local `.sent` marker recording a successful upload |
| `totalImagesFailed` | Failed image transfer attempts during the current application run; repeated failures for the same image can each increase this count |

Use `totalImagesTransfered` exactly as written, with one `r` in `Transfered`. Pending uploads can be calculated as `totalImagesTaken - totalImagesTransfered`. The failed-attempt count is separate from that calculation and resets when the application restarts. The other two counts are read from the local image files and markers on each request, so they can change if files are removed.

This command reports upload status; it does not start an upload or check the remote server's files. Its counts may differ from `total_shots`, which reports captures during the current application run.

For `total_shots` and `picture_uploads`, a caught error returns the same `cmdType` with `cmdData.status` set to `"failure"` and the error text in `cmdData.reason`. The `data` field is omitted. All three queries are on demand and are not part of the regular 60-second updates.

### Update SFTP settings

Send on `widelapse/remoteCommands/0/<clientId>`:

```json
{
  "cmdType": "sftp_information",
  "cmdData": {
    "serverAddr": "192.0.2.10",
    "port": 22,
    "username": "widelapse",
    "password": "example-password",
    "remoteDirectoryPath": "/media/widelapse/site-01"
  }
}
```

All five fields are required. Send the complete settings on each update.

| Field | Value |
| --- | --- |
| `serverAddr` | Non-empty string containing the server's IPv4 address. The current connection code does not resolve hostnames. |
| `port` | Integer from `1` to `65535`; send `22` for the standard SFTP port |
| `username` | Non-empty string |
| `password` | Non-empty string |
| `remoteDirectoryPath` | Non-empty string containing the full destination directory path on the server |

Success reply on `widelapse/remoteCommands/1/<clientId>`:

```json
{
  "cmdType": "sftp_information",
  "cmdData": {
    "status": "success",
    "reason": ""
  }
}
```

Success means the device updated its in-memory settings. The command does not test the server connection, verify the credentials or destination directory, or start an upload. It also does not disconnect an existing SFTP session; the new server address, port, and credentials are used on the next connection. These settings are not currently saved to disk by this command, so the cloud needs to send them again after an application restart.

Failure reply example for an empty field or an out-of-range port:

```json
{
  "cmdType": "sftp_information",
  "cmdData": {
    "status": "failure",
    "reason": "one or more fields are empty inside the payload"
  }
}
```

Missing fields or incorrect JSON types can produce other error text. Use `status` to determine success and display or log the returned `reason`.

### Read SFTP settings

Send on `widelapse/remoteCommands/0/<clientId>`:

```json
{
  "cmdType": "get_sftp_information",
  "cmdData": {}
}
```

Example reply on `widelapse/remoteCommands/1/<clientId>`:

```json
{
  "cmdType": "sftp_information",
  "cmdData": {
    "status": "success",
    "data": {
      "savedServer": "192.0.2.10",
      "savedPort": 22,
      "savedUsername": "widelapse",
      "savedPassword": "example-password",
      "savedRemoteDirectoryPath": "/media/widelapse/site-01"
    },
    "reason": ""
  }
}
```

The reply currently uses `sftp_information`, not `get_sftp_information`, as its `cmdType`. Keep track of the outstanding request when matching this response. These are the current in-memory settings; the `saved` prefix does not mean they have been persisted to disk or that a connection has succeeded.

The response includes the password as `savedPassword`, so treat this payload as credentials and mask the password in the cloud UI and logs.

If reading the settings throws an error, the device returns `cmdType: "sftp_information"` with `cmdData.status: "failure"`, the error text in `cmdData.reason`, and no `data` field. This query runs on demand and is not included in the regular updates.
