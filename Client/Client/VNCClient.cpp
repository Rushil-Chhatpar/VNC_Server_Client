#include "stdafx.h"
#include "VNCClient.h"
#include <qpainter.h>

VNCClient::VNCClient(QWidget* parent)
	: QWidget(parent)
{
	_socket = new QTcpSocket(this);
	_updateTimer = new QTimer(this);

	_updateTimer->setInterval(TIMER_INTERVAL);
	connect(_updateTimer, &QTimer::timeout, this, &VNCClient::updateFrameBuffer);
	_updateTimer->start();
	initConnection();
}

VNCClient::~VNCClient()
{
	_socket->disconnectFromHost();
}

void VNCClient::paintEvent(QPaintEvent* event)
{
	Q_UNUSED(event);

	// painting on this widgtet
	QPainter painter(this);

	if (!_frameBuffer.isNull())
	{
		QImage img = _frameBuffer.scaled(this->size(), Qt::KeepAspectRatio);

		// TODO: Center the image
		QPoint drawPoint((width() - img.width()) / 2, (height() - img.height()) / 2);
		
		painter.drawImage(drawPoint, img);
	}
	else
	{
		painter.drawText(rect(), "No buffer received by the client!!!");
	}
}

void VNCClient::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
}

void VNCClient::onReadReady()
{
	QByteArray data = _socket->readAll();
	if (data.size() > 0)
		processData(data);
}

void VNCClient::onConnected()
{
}

void VNCClient::updateFrameBuffer()
{
	// simple invoke the paint event
	onReadReady();
	update();
}

void VNCClient::initConnection()
{
	QString serverIP = "192.168.2.17";
	int port = 5900;
	_socket->connectToHost(serverIP, port);
}

void VNCClient::processData(const QByteArray& data)
{
	// hardcoded w and h for now
	int width = 1280, height = 720;

	_frameBuffer = QImage(reinterpret_cast<const uchar*>(data.constData()), width, height, QImage::Format_RGBA8888).copy();

	update();
}
