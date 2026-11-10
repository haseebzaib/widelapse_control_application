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

For this first stage, the backend needs to register devices, send the registration acknowledgement, and store incoming network and diagnostic updates against each device.
