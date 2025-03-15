#pragma once

#include <../../include/QtWidgets/qmainwindow.h>

#include <rfb/rfbclient.h>

#include <VNCClient.h>
#include <../../include/QtWidgets/qlabel.h>
#include "VNCClientWrapper.h"

class MainWindow : public QLabel
{
	Q_OBJECT;
public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow();

	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;

	void mouseMoveEvent(QMouseEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

	void resizeEvent(QResizeEvent* event) override;

	void setVNCClient(VNCClientWrapper* client) { _client = client; }

private:
	VNCClientWrapper* _client;
	// just some random number
	const int _buttonMaskHashPress = 91;
	const int _buttonMaskHashRelease = 92;
};