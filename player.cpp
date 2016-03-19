/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Thomas COIN
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "player.h"
#include <QDir>

#include <QGlib/Connect>
#include <QGlib/Error>
#include <QGst/Pipeline>
#include <QGst/ElementFactory>
#include <QGst/Bus>
#include <QGst/Message>
#include <QGst/ClockTime>
#include <QGst/Sample>
#include <QGst/Buffer>
#include <QMessageBox>

#include <glib-object.h>
#include <gst/gstsample.h>
#include <gst/gstcaps.h>
#include <gst/video/video.h>

Player::Player(QWidget *parent) :
    QGst::Ui::VideoWidget(parent)
{
    // Create gstreamer elements
    m_pipeline      = QGst::Pipeline::create("Video Player + Snapshots pipelin");
    m_source        = QGst::ElementFactory::make("videotestsrc", "videotestsrc");
    m_videoSink     = QGst::ElementFactory::make("ximagesink", "video-sink");

    if (!m_pipeline || !m_source || !m_videoSink) {
        QMessageBox::critical(
                    this,
                    "Error",
                    "One or more elements could not be created. Verify that you have all the necessary element plugins installed."
                    );
        return;
    }

    m_videoSink->setProperty("enable-last-sample", true);

    // Add elements to pipeline
    m_pipeline->add(m_source);
    m_pipeline->add(m_videoSink);

    // Link elements
    m_source->link(m_videoSink);

    watchPipeline(m_pipeline);
    setAutoFillBackground(true);

    // Connect to pipeline's bus
    QGst::BusPtr bus = m_pipeline->bus();
    bus->addSignalWatch();
    QGlib::connect(bus, "message", this, &Player::onBusMessage);
    bus.clear();

    m_pipeline->setState(QGst::StatePlaying);
}

Player::~Player()
{
    if (m_pipeline) {
        m_pipeline->setState(QGst::StateNull);
        stopPipelineWatch();
        m_pipeline.clear();
    }
}

void Player::takeSnapshot()
{
    QDateTime currentDate = QDateTime::currentDateTime();
    QString location = QString("%1/snap_%2.png").arg(QDir::homePath()).arg(currentDate.toString(Qt::ISODate));
    QImage snapShot;
    QImage::Format snapFormat;
    QGlib::Value val = m_videoSink->property("last-sample");
    GstSample *videoSample = (GstSample *)g_value_get_boxed(val);
    QGst::SamplePtr sample = QGst::SamplePtr::wrap(videoSample);
    QGst::SamplePtr convertedSample;
    QGst::BufferPtr buffer;
    QGst::CapsPtr caps = sample->caps();
    QGst::MapInfo mapInfo;
    GError *err = NULL;
    GstCaps * capsTo = NULL;
    const QGst::StructurePtr structure = caps->internalStructure(0);
    int width, height;

    width = structure.data()->value("width").get<int>();
    height = structure.data()->value("height").get<int>();

    qDebug() << "Sample caps:" << structure.data()->toString();

    /*
     * { QImage::Format_RGBX8888, GST_VIDEO_FORMAT_RGBx  },
     * { QImage::Format_RGBA8888, GST_VIDEO_FORMAT_RGBA  },
     * { QImage::Format_RGB888  , GST_VIDEO_FORMAT_RGB   },
     * { QImage::Format_RGB16   , GST_VIDEO_FORMAT_RGB16 }
     */
    snapFormat = QImage::Format_RGB888;
    capsTo = gst_caps_new_simple("video/x-raw",
                                 "format", G_TYPE_STRING, "RGB",
                                 "width", G_TYPE_INT, width,
                                 "height", G_TYPE_INT, height,
                                 NULL);

    convertedSample = QGst::SamplePtr::wrap(gst_video_convert_sample(videoSample, capsTo, GST_SECOND, &err));
    if (convertedSample.isNull()) {
        qWarning() << "gst_video_convert_sample Failed:" << err->message;
    }
    else {
        qDebug() << "Converted sample caps:" << convertedSample->caps()->toString();

        buffer = convertedSample->buffer();
        buffer->map(mapInfo, QGst::MapRead);

        snapShot = QImage((const uchar *)mapInfo.data(),
                          width,
                          height,
                          snapFormat);

        qDebug() << "Saving snap to" << location;
        snapShot.save(location);

        buffer->unmap(mapInfo);
    }

    val.clear();
    sample.clear();
    convertedSample.clear();
    buffer.clear();
    caps.clear();
    g_clear_error(&err);
    if (capsTo)
        gst_caps_unref(capsTo);
}

/***************************************************************************/
/********************************* PRIVATE *********************************/
/***************************************************************************/

void Player::onBusMessage(const QGst::MessagePtr &message)
{
    switch (message->type()) {
    case QGst::MessageEos: //End of stream. We reached the end of the file.
        qDebug() << Q_FUNC_INFO << "EOS";
        break;
    case QGst::MessageError: //Some error occurred.
        qCritical() << message.staticCast<QGst::ErrorMessage>()->error();
        break;
    case QGst::MessageStateChanged: //The element in message->source() has changed state
        if (message->source() == m_pipeline)
            handlePipelineStateChange(message.staticCast<QGst::StateChangedMessage>());
        break;
    default:
        break;
    }
}

void Player::handlePipelineStateChange(const QGst::StateChangedMessagePtr &scm)
{
    switch (scm->newState()) {
    case QGst::StatePlaying:
        qDebug() << "New state :" << "StatePlaying";
        break;
    case QGst::StatePaused:
        qDebug() << "New state :" << "StatePaused";
        break;
    case QGst::StateNull:
        qDebug() << "New state :" << "StateNull";
        break;
    case QGst::StateReady:
        qDebug() << "New state :" << "StateReady";
        break;
    default:
        break;
    }
}
