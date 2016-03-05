# qtgstreamer-snapshot-taker-example

A simple application based on [QtGStreamer](https://gstreamer.freedesktop.org/modules/qt-gstreamer.html) which can play a video and take snapshots.

A simple GStreamer `pipeline` is used, described above:
```
                                                  ---------   -----------------
                                              /-->| queue |-->| autovideosink |
----------------   -------------   -------   /    --------    -----------------
| videotestsrc |-->| decodebin |-->| tee |--|
----------------   -------------   -------   \    ---------   ----------------   ---------    ------------
                                              \-->| queue |-->| videoconvert |-->| pngenc |-->| filesink |
                                                  ---------   ----------------   ----------   ------------
```
