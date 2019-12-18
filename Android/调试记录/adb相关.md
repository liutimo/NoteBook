1. 开启网络ADB

    ```shell
    setprop service.adb.tcp.port 5555
    stop adbd
    start adbd
    ```

2. android7.0 adb root

    ```shell
    adb disable-verity 
    adb reboot
    adb root
    adb remount
    ```

    