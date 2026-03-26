# 一、Introduction

rtsnn library is a neural network library developed by REALTEK. Odnano is a lightweight object detection algorithm.

# 二、Execute demo program

Place the package `rtsnn_odnano` on the board:

1. Place libraries such as lib/librtsnn.so in the package directory/usr/lib
2. Place library files such as `plugin/librtsnn_odnano.so in the package into the/usr/share/rtsnn plug-in directory
3. Place the demo program in the package in the/usr/bin directory

After the environment is ready, you can run the demo program as follows. If the target is detected, it will print the target and its coordinates on the serial port:

```
# example_od odnano /usr/share/rtsnn/librtsnn_odnano.so 256 160

frame 0
od person, prob: 0.56, (95, 0) (192, 151)
od face, prob: 0.33, (149, 0) (180, 50)

frame 1
od person, prob: 0.53, (95, 0) (191, 151)
od face, prob: 0.28, (151, 0) (182, 52)

frame 2
od person, prob: 0.51, (94, 0) (192, 151)
od face, prob: 0.27, (156, 0) (185, 62)
```

* Note *: The pure C version of odtiny does not require a data file. The second parameter can be an arbitrary string.

# 三、Development

You can refer to `example_od.c` to use odnano library, which mainly has the following three APIs:
1. rts_nn_init: Initialize the network
2. rts_nn_od_run:  Execute the network for each frame of data to perform object detection
3. rts_nn_release: Release network resources
When using the rtsnn static library, use these compilation options: -WL,--export-dynamic -WL,--whole-archive librtsnn.a -WL,--no-whole-archive
