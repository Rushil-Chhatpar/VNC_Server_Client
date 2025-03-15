#pragma once

#include <../../include/QtWidgets/qwidget.h>
#include <qtcpsocket.h>
#include <qimage.h>
#include <qtimeline.h>
#include <qtimer.h>

const int TIMER_INTERVAL = 30;


class VNCClient : public QWidget
{
	Q_OBJECT
public:
	VNCClient(QWidget* parent);
	~VNCClient();

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

private slots:
    void onConnected();
    void onReadReady();
    void updateFrameBuffer();

private:
    void initConnection();
    void processData(const QByteArray& data);
	QTcpSocket* _socket;
	QImage _frameBuffer;
	QTimer* _updateTimer;
};