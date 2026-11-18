# widelapse control application


pkill -f gvfsd-gphoto2
pkill -f gvfs-gphoto2-volume-monitor

mkdir build

cd build

cmake ..
cmake --build . -j$(nproc)

Schedule intervals use `std::chrono::seconds`. Use `seconds(30)` for every
30 seconds, `minutes(5)` for every 5 minutes, or `hours(1)` for every hour.
Minutes and hours convert automatically to seconds. Schedule start and end
times remain minutes since midnight.


Cloud scheduling uses a complete replacement command:

```json
{
  "cmdType": "scheduling_setup",
  "cmdData": {
    "schedule": [
      {
        "type": "camera",
        "weekDays": [-1],
        "startTime": 1350,
        "endTime": 1440,
        "interval": 30
      },
      {
        "type": "transfer",
        "weekDays": [-1],
        "startTime": 0,
        "endTime": 1440,
        "interval": 600
      }
    ]
  }
}
```

`weekDays` uses Sunday=0 through Saturday=6; `[-1]` or `[]` means every day.
Start/end are minutes since midnight in device local time, with start included
and end excluded. The example captures from 22:30 until midnight every 30 seconds
and transfers all day every 10 minutes. Intervals must be positive integer seconds.
Overnight windows require two entries.

The complete command JSON is saved at `/opt/widelapse/configs/scheduling.json`
and loaded before MQTT starts on each application startup. Every new command
replaces both scheduler lists; profiles omitted from the command are removed.
An empty `schedule` array clears and saves both lists. The entire list is checked
before saving or replacing profiles; invalid input returns a failure reply and
keeps the previous configuration. No hardcoded profiles are installed if the
saved file is missing or invalid. Already selected/running callbacks may finish
during a replacement. New profiles run immediately when inside their window,
then repeat at their configured interval.
