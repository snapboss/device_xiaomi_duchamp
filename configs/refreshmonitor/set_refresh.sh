#!/system/bin/sh

case "$1" in
    60) service call SurfaceFlinger 1035 i32 0 ;;
    120) service call SurfaceFlinger 1035 i32 2 ;;
    90) service call SurfaceFlinger 1035 i32 3 ;;
    *)
        echo "Unhandled rate: $1" >> /data/local/tmp/refresh_monitor.log
        ;;
esac


#!/system/bin/sh

case "$1" in
    30) 
        service call SurfaceFlinger 1035 i32 0
        ;;
    60) 
        service call SurfaceFlinger 1035 i32 0  # Adjust if needed for 90Hz
        ;;
    90) 
        service call SurfaceFlinger 1035 i32 3
        ;;
    120) 
        service call SurfaceFlinger 1035 i32 2
        ;;
    *)
        echo "Unhandled rate: $1" >> /data/local/tmp/refresh_monitor.log
        ;;
esac



