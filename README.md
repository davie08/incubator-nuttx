# nuttx for ttgo twatch

环境准备:

    xtensa-esp32-elf-gcc 编译器，可以从esp idf工程(https://github.com/espressif/esp-idf.git)里的install.sh安装，
    或直接去https://github.com/espressif/crosstool-NG/releases下载。
    
    本地CP2104串口驱动，插上micro usb后，本地应能看到诸如/dev/tty.SLAB_USBtoUART的串口节点。
    
    
编译lvgldemo:

    git clone git@github.com:davie08/incubator-nuttx.git -b ttgo nuttx
    git clone git@github.com:apache/incubator-nuttx-apps.git apps
    cd nuttx
    ./tools/configure.sh ttgo_twatch_esp32:default
    make
    make download ESPTOOL_PORT=/dev/tty.SLAB_USBtoUART
    

运行lvgldemo:

    串口进shell直接运行lvgldemo
    
