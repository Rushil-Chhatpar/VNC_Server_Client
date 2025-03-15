#include "stdafx.h"
#include <thread>
#include <mutex>
#include <condition_variable>



#include <QGuiApplication>
#include <../include/QtWidgets/qapplication.h>
#include <../include/QtWidgets/qmainwindow.h>
#include <../include/QtWidgets/qlayout.h>
#include <QQmlApplicationEngine>
#include <qqmlcontext.h>
#include <qopenglbuffer.h>
#include <QtOpenGLWidgets/qopenglwidget.h>
#include <qobject.h>
#include <qtcpsocket.h>
#include <qthread.h>
#include <../../include/QtWidgets/qlabel.h>

#include <ws2tcpip.h>
#include <signal.h>
#include <rfb/rfbclient.h>


#include "OpenGLWidget.h"
#include "ClientThread.h"
#include "MainWindow.h"
#include "VNCClient.h"
#include "VNCClientWrapper.h"

//void ClientRecvMsgUpdt(rfbClient* client, OpenGLWidget* widget);
//
//void updateFrameBuffer(rfbClient* client, int x, int y, int w, int h)
//{
//	char* data = nullptr;
//	ReadFromRFBServer(client, data, 0);
//	client->frameBuffer;
//	int bp = 0;
//}
//
//std::mutex mtx;
//
//int main(int argc, char **argv)
//{
//#if defined(Q_OS_WIN) && QT_VERSION_CHECK(5, 6, 0) <= QT_VERSION && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
//    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
//#endif
//
//    QApplication app(argc, argv);
//	QMainWindow window;
//	window.setWindowTitle("VNC Client");
//	window.resize(1280, 720);
//
//	std::shared_ptr<OpenGLWidget> glWidget = std::make_shared<OpenGLWidget>(&window);
//	window.setCentralWidget(glWidget.get());
//
//	window.resize(1280, 720);
//
// //   QOpenGLBuffer buffer(QOpenGLBuffer::PixelPackBuffer);
//	//buffer.create();
//
//    //QQmlApplicationEngine engine;
//    //engine.load(QUrl(QStringLiteral("qrc:/qt/qml/client/main.qml")));
//    //if (engine.rootObjects().isEmpty())
//    //    return -1;
//
//    // VNC Client
//    rfbClient* _client;
//    SDL_Event _sdlEvent;
//
//	SDL_InitSubSystem(SDL_INIT_VIDEO);
//	atexit(SDL_Quit);
//	signal(SIGINT, exit);
//
//	_client = rfbGetClient(8, 3, 4);
//	if (!_client)
//	{
//		qDebug() << "Failed to create client!\n";
//		return -1;
//	}
//
//	const char* serverIP = "192.168.2.17";
//	int port = 5900;
//
//	_client->serverHost = strdup(serverIP);
//	_client->serverPort = port;
//	_client->GotFrameBufferUpdate = updateFrameBuffer;
//	_client->appData.useRemoteCursor = FALSE;
//	_client->appData.enableJPEG = FALSE;
//
//	if (rfbInitClient(_client, nullptr, nullptr))
//	{
//		std::cout << "Connected successfully\n";
//	}
//	else
//	{
//		qDebug() << "Connection failed!\n";
//	}
//    
//	//std::thread clientRecvUpdt(ClientRecvMsgUpdt, _client, glWidget.get());
//	//clientRecvUpdt.detach();
//
//	QThread* workerThread = new QThread(&app);
//	ClientWorker* worker = new ClientWorker(_client);
//	worker->moveToThread(workerThread);
//
//	// Connect signals
//	QObject::connect(workerThread, &QThread::started,
//		worker, &ClientWorker::doReceiveLoop);
//	QObject::connect(worker, &ClientWorker::frameReady,
//		glWidget.get(), &OpenGLWidget::updateFram);
//
//	// Start the thread
//	workerThread->start();
//
//
//	////rfbClientCleanup(_client);
//	//while (_client->sock)
//	//{
//	//	if (WaitForMessage(_client, 1000) > 0)
//	//	{
//	//		if (!HandleRFBServerMessage(_client))
//	//		{
//	//			qDebug("Failed to handle server msg!");
//	//		}
//	//	}
//	//}
//	window.show();
//	app.exec();
//	workerThread->quit();
//	return 0;
//}
//
//void ClientRecvMsgUpdt(rfbClient* client, OpenGLWidget* widget)
//{
//	mtx.lock();
//	while (client->sock)
//	{
//		rfbRectangle rect;
//		rfbBool rfbbool;
//		rfbClientGetUpdateRect(client, &rect, &rfbbool);
//		size_t size = 1280 * 720 * 4;
//		std::vector<uint8_t> data(client->frameBuffer, client->frameBuffer + size);
//		QImage image(reinterpret_cast<const uchar*>(data.data()), 1280, 720, QImage::Format_RGBA8888);
//		widget->updateFram(image);
//		if (WaitForMessage(client, 1000) > 0)
//		{
//			if (!HandleRFBServerMessage(client))
//			{
//				qDebug("Failed to handle server msg!");
//			}
//		}
//	}
//	mtx.unlock();
//}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	int windowWidth = 1280, windowHeight = 720;
	MainWindow w;
	w.setWindowTitle("VNC Client");
	w.resize(windowWidth, windowHeight);
	w.show();
	w.setFocusPolicy(Qt::StrongFocus);

	std::shared_ptr<VNCClientWrapper> vncClient = std::make_shared<VNCClientWrapper>(windowWidth, windowHeight);
	w.setVNCClient(vncClient.get());

	QTimer timer;
	uint8_t timeOutCount = 0;
	uint8_t timeOutLimit = 10;
	QObject::connect(&timer, &QTimer::timeout, [&]() {
		
		int width = windowWidth;
		int height = windowHeight;
		if (!vncClient->processEvents())
		{
			timeOutCount++;
			if (timeOutCount >= timeOutLimit)
			{
				// reset
				vncClient.reset();
				vncClient = std::make_shared<VNCClientWrapper>(width, height);
				w.setVNCClient(vncClient.get());
				timeOutCount = 0;
			}
		}
		//vncClient.receiveDataFromServer();
		
		// resize window
		vncClient->getFBSize(width, height);
		if (windowWidth != width || windowHeight != height)
		{
			windowWidth = width; windowHeight = height;
			w.resize(width, height);
		}

		QImage image = vncClient->getFrameBufferImage();
		if (!image.isNull())
			w.setPixmap(QPixmap::fromImage(image));
		else
			qDebug("Framebuffer is null");
		});
	timer.start(30);

	return app.exec();
}