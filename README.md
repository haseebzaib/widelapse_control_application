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

