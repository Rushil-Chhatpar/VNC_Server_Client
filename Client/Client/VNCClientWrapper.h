#pragma once

#include <../../include/QtWidgets/qwidget.h>
#include <qtcpsocket.h>
#include <qimage.h>
#include <qtimeline.h>
#include <qtimer.h>
#include <rfb/rfbclient.h>

struct ivec2
{
	int x = 0;
	int y = 0;
};

struct ivec2x2
{
	ivec2 v1;
	ivec2 v2;
};

class VNCClientWrapper : public QObject
{
	Q_OBJECT
public:
	explicit VNCClientWrapper(int width, int height, QObject* parent = nullptr);
	~VNCClientWrapper();

	bool processEvents();
	QImage getFrameBufferImage();
	void receiveDataFromServer();

	void sendKeyEventToServer(int key, bool isPressed);
	void sendMouseEventToServer(int x, int y, int buttonMask = 0);
	void sendMousePosToServer(int x, int y);

	void sendResizeToServer(int width, int height);

	int getFBWidth();
	int getFBHeight();
	void getFBSize(int& width, int& height);

private:
	void* frameBufferAlloc(rfbClient* client, int width, int height);
	void updateNewFrameBuffer(int width, int height);

private:
	rfbClient* _client;
	const std::string _serverIP = "192.168.2.17";
	const int _port = 5900;
	const int _localUdpPort = 55556;
	const int _udpPort = 55555;
	SOCKET _udpSocket;
	sockaddr_in _serverAddr;

	const size_t _maxUdpPayload = 20000;

	ivec2 _fbSize;
};