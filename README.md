# widelapse control application


pkill -f gvfsd-gphoto2
pkill -f gvfs-gphoto2-volume-monitor

mkdir build

cd build

cmake ..
cmake --build . -j$(nproc)

