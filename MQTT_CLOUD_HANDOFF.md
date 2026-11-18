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

The cloud can send Wi-Fi settings and capture/transfer schedules to a registered device. Both commands use the same topics:

| Direction | Topic |
| --- | --- |
| Cloud → device: command | `widelapse/remoteCommands/0/<clientId>` |
| Device → cloud: result | `widelapse/remoteCommands/1/<clientId>` |

For example, publish commands for `widelapse_aabbccddeeff` to `widelapse/remoteCommands/0/widelapse_aabbccddeeff` and listen for results on `widelapse/remoteCommands/1/widelapse_aabbccddeeff`. The backend can subscribe to `widelapse/remoteCommands/1/+` for all devices.

Subscribe for results before sending a command. Publish with QoS 1 and retain disabled so old commands are not replayed when a device connects. The device subscribes and replies at QoS 1, with replies not retained. Commands are processed after registration is acknowledged.

Requests contain `cmdType` and `cmdData`. Replies return the same `cmdType`, with `status` and `reason` inside `cmdData`. Command names are case-sensitive: use exactly `wifiSetup` and `scheduling_setup`.

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
