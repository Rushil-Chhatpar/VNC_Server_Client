#pragma once

#include <qthread.h>

#include <QImage>
#include <rfb/rfbclient.h>

class ClientWorker : public QObject
{
    Q_OBJECT
public:
    ClientWorker(rfbClient* client) : _client(client) {}

public slots:
    void doReceiveLoop()
    {
        while (_client->sock)
        {
            // Wait for VNC data
            if (WaitForMessage(_client, 1000) > 0)
            {
                if (!HandleRFBServerMessage(_client))
                {
                    qDebug("Failed to handle server msg!");
                    continue;
                }
            }

            // Grab the updated framebuffer
            size_t size = 1280 * 720 * 4;
            std::vector<uint8_t> data(_client->frameBuffer, _client->frameBuffer + size);
            QImage image((const uchar*)data.data(), 1280, 720, QImage::Format_RGBA8888);
            emit frameReady(image.copy());
            // .copy() to ensure QImage is valid beyond the raw pointer lifetime
        }
    }

signals:
    void frameReady(const QImage& image);

private:
    rfbClient* _client;
};