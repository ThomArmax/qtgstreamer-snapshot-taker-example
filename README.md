# qtgstreamer-snapshot-taker-example

A simple application based on [QtGStreamer](https://gstreamer.freedesktop.org/modules/qt-gstreamer.html) which can play a video and take snapshots.

The create pipeline is quite simple:
```
----------------   --------------
| videotestsrc |-->| ximagesink |
----------------   --------------
```

Basically, We use the `last-sample` property of `ximagesink` to get a `GstSample`.
This sample is converted in the desired format, using `gst_video_convert_sample`.
Then, the converted sample is written into a `QImage`
