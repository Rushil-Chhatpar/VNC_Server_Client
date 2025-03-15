#include "stdafx.h"
#include "VNCClientWrapper.h"
#include <winsock2.h>
#include <WS2tcpip.h>
#include <qmutex.h>

VNCClientWrapper::VNCClientWrapper(int width, int height, QObject* parent)
	: QObject(parent)
	, _client(nullptr)
{
	_client = rfbGetClient(8, 3, 4);
	if (!_client)
	{
		qFatal("Failed to create rfbClient instances.");
	}

	_client->serverHost = strdup(_serverIP.c_str());
	_client->serverPort = _port;
	_client->canHandleNewFBSize = TRUE;
	
	std::string tag = "NULLZEC";
	_fbSize.x = width;
	_fbSize.y = height;
	//_client->clientData
	//_client->clientData = new rfbClientData();
	//_client->clientData->tag = &tag;
	//_client->clientData->data = &fbSize;
	
	//_client->MallocFrameBuffer = frameBufferAlloc;

	if (!rfbInitClient(_client, nullptr, nullptr))
	{
		qWarning("failed to init client!!!");
		exit(1);
	}

	if (_client->sock != INVALID_SOCKET)
	{
		int opt = 1;
		int res = setsockopt(_client->sock, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
		if (res == 0)
		{
			qDebug() << "TCP_NODELAY set successfully\n";
		}
		else
		{
			qFatal() << "TCP_NODELAP cannot be set: " << WSAGetLastError();
		}
	}

	_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (_udpSocket == INVALID_SOCKET)
	{
		qFatal() << "Failed to create udp socket!!!";
	}

	sockaddr_in localAddr;
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = INADDR_ANY;
	localAddr.sin_port = htons(_localUdpPort);

	int res = bind(_udpSocket, (SOCKADDR*)&localAddr, sizeof(localAddr));
	if (res == SOCKET_ERROR)
	{
		qFatal() << "Failed to bind udp socket!!!";
	}
	// NON BLOCKING
	u_long arg = 1;
	ioctlsocket(_udpSocket, FIONBIO, &arg);

	_serverAddr.sin_family = AF_INET;
	_serverAddr.sin_addr.s_addr = inet_addr(_serverIP.c_str());
	_serverAddr.sin_port = htons(_udpPort);
	if(::connect(_udpSocket, (SOCKADDR*)&_serverAddr, sizeof(_serverAddr)) == SOCKET_ERROR)
	{
		qFatal() << "Failed to connect udp socket!!!";
	}
}

VNCClientWrapper::~VNCClientWrapper()
{
	if (_client)
	{
		rfbClientCleanup(_client);
		_client = nullptr;
	}
	if (_udpSocket != INVALID_SOCKET)
	{
		closesocket(_udpSocket);
	}
}

bool VNCClientWrapper::processEvents()
{
	if (!_client)
		return false;

	int res = WaitForMessage(_client, 5000);
	if (res < 0)
	{
		qWarning("WaitForMessage error!!!");
		return false;
	}

	if (res > 0)
	{
		if (!HandleRFBServerMessage(_client))
		{
			//Sleep(10);
			//SendFramebufferUpdateRequest(_client, 0, 0, _client->width, _client->height, false);
			qWarning("HandleRFBServerMessage error!!!");
			return false;
		}
	}

	return true;
}

QImage VNCClientWrapper::getFrameBufferImage()
{
	
	if (!_client || !_client->frameBuffer)
		return QImage();

	QImage image(reinterpret_cast<uchar*>(_client->frameBuffer), 
		_client->width, _client->height, 
		QImage::Format_RGB32);
	if (!image.isNull())
		image.mirror(false, true);
	
	return image.copy();
}

void VNCClientWrapper::receiveDataFromServer()
{
	if (!_client || !_client->frameBuffer)
		return;

	const int width = _client->width;
	const int height = _client->height;
	//char* buffer = new char[1280 * 720 * 4];
	int fbSize = width * height * 4;
	std::vector<char> buffer(fbSize, 0);
	socklen_t serverAddrLen = sizeof(_serverAddr);

	// save some memory
#pragma pack(push, 1)
	struct ChunkHeader
	{
		uint32_t frameId;
		uint32_t totalChunks;
		uint32_t chunkIndex;
		uint16_t payloadSize;
	};
#pragma pack(pop)

	uint32_t frameId = 0;
	uint32_t totalChunks = 0;
	bool firstPacket = true;
	std::vector<bool> ChunkBuffer;
	uint32_t chunksReceived = 0;

	const int maxLoops = (width * height * 4) / _maxUdpPayload;
	int counter = 0;
	while (counter < maxLoops)
	{
		counter++;

		std::vector<char> receiveBuffer(_maxUdpPayload, 0);
		int bytesRecvd = recvfrom(_udpSocket, receiveBuffer.data(), (int)receiveBuffer.size(), 0, (SOCKADDR*)&_serverAddr, &serverAddrLen);
		if (bytesRecvd == SOCKET_ERROR || bytesRecvd == 0)
		{
			continue;
		}

		if (bytesRecvd < (int)sizeof(ChunkHeader))
			continue;

		ChunkHeader chunkHeader;
		memcpy(&chunkHeader, receiveBuffer.data(), sizeof(ChunkHeader));

		if (firstPacket)
		{
			frameId = chunkHeader.frameId;
			totalChunks = chunkHeader.totalChunks;
			ChunkBuffer.resize(totalChunks, false);
			firstPacket = false;
		}

		// invalid
		if (chunkHeader.frameId != frameId || chunkHeader.chunkIndex >= totalChunks)
			continue;

		int payLoadSize = chunkHeader.payloadSize;
		int actualPayload = bytesRecvd - sizeof(ChunkHeader);
		if (payLoadSize > actualPayload)
			payLoadSize = actualPayload;

		size_t chunkSize = _maxUdpPayload - sizeof(ChunkHeader);
		size_t offset = chunkHeader.chunkIndex * chunkSize;
		// if last packet
		if (offset + payLoadSize > fbSize)
			payLoadSize = fbSize - offset;

		memcpy(buffer.data() + offset, receiveBuffer.data() + sizeof(ChunkHeader), payLoadSize);

		// edit the hashtable
		if (!ChunkBuffer[chunkHeader.chunkIndex])
		{
			ChunkBuffer[chunkHeader.chunkIndex] = true;
			chunksReceived++;
		}
		
		// all packets received
		if (chunksReceived >= totalChunks)
			break;

	}
	memcpy(_client->frameBuffer, buffer.data(), fbSize);
	
	//int receivedBytes = recvfrom(_udpSocket, buffer, sizeof(buffer), 0, (SOCKADDR*)&_serverAddr, &serverAddrLen);
	//if (receivedBytes > 0)
	//{
	//	std::memcpy(_client->frameBuffer, buffer, receivedBytes);
	//}
	//delete[] buffer;
}

void VNCClientWrapper::sendKeyEventToServer(int key, bool isPressed)
{
	if (_client)
	{
		SendKeyEvent(_client, key, isPressed ? TRUE : FALSE);
	}
}

void VNCClientWrapper::sendMouseEventToServer(int x, int y, int buttonMask)
{
	if (_client)
	{
		SendPointerEvent(_client, x, y, buttonMask);
	}
}

void VNCClientWrapper::sendMousePosToServer(int x, int y)
{
	ivec2 mousePos;
	mousePos.x = x; mousePos.y = y;
	socklen_t serverAddrLen = sizeof(_serverAddr);
	int bytesSent = sendto(_udpSocket, reinterpret_cast<const char*>(&mousePos), sizeof(ivec2), 0, (SOCKADDR*)&_serverAddr, (int)serverAddrLen);
	if (bytesSent == SOCKET_ERROR)
	{
		qWarning() << "Failed to send mouse pos to the server!!!";
	}
}

void VNCClientWrapper::sendResizeToServer(int width, int height)
{
	if (!_client)
		return;
	ivec2 fbSize;
	fbSize.x = width; fbSize.y = height;
	ivec2 curSize;
	curSize.x = _client->width; curSize.y = _client->height;
	ivec2x2 size;
	size.v1 = curSize; size.v2 = fbSize; // old and new
	socklen_t serverAddrLen = sizeof(_serverAddr);
	int bytesSent = sendto(_udpSocket, reinterpret_cast<const char*>(&size), sizeof(ivec2x2), 0, (SOCKADDR*)&_serverAddr, (int)serverAddrLen);
	if (bytesSent == SOCKET_ERROR)
	{
		qWarning() << "Failed to send new fb size!!!";
	}
	updateNewFrameBuffer(width, height);
}

int VNCClientWrapper::getFBWidth()
{
	if (_client)
	{
		ivec2* data;
		data = reinterpret_cast<ivec2*>(_client->clientData->data);
		return data->x;
	}
	return 0;
}

int VNCClientWrapper::getFBHeight()
{
	if (_client)
	{
		ivec2* data;
		data = reinterpret_cast<ivec2*>(_client->clientData->data);
		return data->y;
	}
	return 0;
}

void VNCClientWrapper::getFBSize(int& width, int& height)
{
	ivec2 fbSize;
	socklen_t serverAddrLen = sizeof(_serverAddr);
	int bytesRecvd = recvfrom(_udpSocket, reinterpret_cast<char*>(&fbSize), sizeof(fbSize), 0, (SOCKADDR*)&_serverAddr, &serverAddrLen);
	int error = WSAGetLastError();
	if (bytesRecvd == SOCKET_ERROR)
	{
		return;
	}
	width = fbSize.x;
	height = fbSize.y;
	updateNewFrameBuffer(width, height);
}

void* VNCClientWrapper::frameBufferAlloc(rfbClient* client, int width, int height)
{
	int bytesPerPixel = client->format.bitsPerPixel / 8;
	return (void*)malloc(width * height * bytesPerPixel);
}

void VNCClientWrapper::updateNewFrameBuffer(int width, int height)
{
	_client->width = width;
	_client->height = height;
	size_t newFBSize = width * height * 4;
	char* buffer = (char*)malloc(newFBSize);
	if (buffer)
	{
		memset(buffer, 0, newFBSize);
	}
	rfbBool res = SendFramebufferUpdateRequest(_client, 0, 0, _client->width, _client->height, false); 
	uint8_t* oldBuffer = _client->frameBuffer;
	_client->frameBuffer = (uint8_t*)buffer;
	free(oldBuffer);
}
