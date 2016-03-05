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
#include <QGlib/Connect>
#include <QGlib/Error>
#include <QGst/Pipeline>
#include <QGst/ElementFactory>
#include <QGst/Bus>
#include <QGst/Message>
#include <QMessageBox>

Player::Player(QWidget *parent) :
    QGst::Ui::VideoWidget(parent)
{
    // Create gstreamer elements
    m_pipeline      = QGst::Pipeline::create("Video Player + Snapshots pipelin");
    m_source        = QGst::ElementFactory::make("videotestsrc", "videotestsrc");
    m_decoder       = QGst::ElementFactory::make("decodebin", "decodebin");
    m_tee           = QGst::ElementFactory::make("tee", "video-tee");
    m_videoQueue    = QGst::ElementFactory::make("queue", "video-queue");
    m_videoSink     = QGst::ElementFactory::make("autovideosink", "video-sink");
    m_snapQueue     = QGst::ElementFactory::make("queue", "snap-queue");
    m_snapConverter = QGst::ElementFactory::make("videoconvert", "snap-converter");
    m_snapEncoder   = QGst::ElementFactory::make("pngenc", "snap-encoder");
    m_snapSink      = QGst::ElementFactory::make("filesink", "snap-filesink");

    if (!m_pipeline || !m_source || !m_decoder || !m_tee || !m_videoQueue || !m_videoSink ||
        !m_snapQueue || !m_snapConverter  || !m_snapEncoder || !m_snapSink) {
        QMessageBox::critical(
                    this,
                    "Error",
                    "One or more elements could not be created. Verify that you have all the necessary element plugins installed."
                    );
        return;
    }

    // Set the snapshot property of png-enc
    m_snapEncoder->setProperty("snapshot", false);

    // Add elements to pipeline
    m_pipeline->add(m_source);
    m_pipeline->add(m_decoder);

    // Link elements
    m_source->link(m_decoder);

    watchPipeline(m_pipeline);
    setAutoFillBackground(true);

    // Connect to pipeline's bus
    QGst::BusPtr bus = m_pipeline->bus();
    bus->addSignalWatch();
    QGlib::connect(bus, "message", this, &Player::onBusMessage);
    bus.clear();

    // Connect on 'pad-added' signal
    QGlib::connect(m_decoder, "pad-added", this, &Player::onPaddedAdded);

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
    QDateTime dateTime = QDateTime::currentDateTime();
    QString snapLocation = QString("/tmp/snap_%1.png").arg(dateTime.toString(Qt::ISODate));

    // Stop the snapshot branch
    m_snapQueue->setState(QGst::StateNull);
    m_snapConverter->setState(QGst::StateNull);
    m_snapEncoder->setState(QGst::StateNull);
    m_snapSink->setState(QGst::StateNull);

    // Set the snapshot location property
    m_snapSink->setProperty("location", snapLocation);

    // Check if we are linked
    if (!m_snapTeeSrcPad->isLinked())
        m_snapTeeSrcPad->link(m_snapQueue->getStaticPad("sink"));

    // Unlock snapshot branch
    m_snapQueue->setStateLocked(false);
    m_snapConverter->setStateLocked(false);
    m_snapEncoder->setStateLocked(false);
    m_snapSink->setStateLocked(false);

    // Synch snapshot branch state with parent
    m_snapQueue->syncStateWithParent();
    m_snapConverter->syncStateWithParent();
    m_snapEncoder->syncStateWithParent();
    m_snapSink->syncStateWithParent();
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

void Player::onPaddedAdded(QGst::PadPtr newPad)
{
    QGst::CapsPtr caps = newPad->currentCaps();
    qDebug() << "Received" << caps->toString() << "caps";

    if (caps->toString().startsWith("video/x-raw")) {
        // Request src pads to Tee
        m_videoTeeSrcPad = m_tee->getRequestPad("src_%u");
        m_snapTeeSrcPad = m_tee->getRequestPad("src_%u");
        m_videoTeeSrcPad->setName("video-tee-src-pad");
        m_snapTeeSrcPad->setName("snap-tee-src-pad");

        // Add video elements into the pipeline
        m_pipeline->add(m_tee);
        m_pipeline->add(m_videoQueue);
        m_pipeline->add(m_videoSink);

        // Link video elements
        m_videoQueue->link(m_videoSink);

        // Add snapshot elements into the pipeline
        m_pipeline->add(m_snapQueue);
        m_pipeline->add(m_snapConverter);
        m_pipeline->add(m_snapEncoder);
        m_pipeline->add(m_snapSink);

        // Link snapshot elements
        m_snapQueue->link(m_snapConverter);
        m_snapConverter->link(m_snapEncoder);
        m_snapEncoder->link(m_snapSink);

        // Lock snapshot branch
        m_snapQueue->setStateLocked(true);
        m_snapConverter->setStateLocked(true);
        m_snapEncoder->setStateLocked(true);
        m_snapSink->setStateLocked(true);

        // Set the video branch to PLAYING state
        m_tee->setState(QGst::StatePlaying);
        m_videoQueue->setState(QGst::StatePlaying);
        m_videoSink->setState(QGst::StatePlaying);

        // Link tee' src pad to video queue sink pad
        m_videoTeeSrcPad->link(m_videoQueue->getStaticPad("sink"));

        // Link received pad to tee' sink pad
        newPad->link(m_tee->getStaticPad("sink"));
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
