#include "stdafx.h"
#include "MainWindow.h"
#include <../../include/QtWidgets/qboxlayout.h>
#include <../../include/QtGui/qevent.h>

MainWindow::MainWindow(QWidget* parent)
	: QLabel(parent)
{
}

MainWindow::~MainWindow()
{
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
	if (_client)
	{
		if (event->key() == Qt::Key_Backspace)
			_client->sendKeyEventToServer(VK_BACK, true);
		else
			_client->sendKeyEventToServer(event->key(), true);
	}
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
	if (_client)
	{
		if (event->key() == Qt::Key_Backspace)
			_client->sendKeyEventToServer(VK_BACK, false);
		else
			_client->sendKeyEventToServer(event->key(), false);
	}
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
	//_client->sendMouseEventToServer(event->pos().x(), event->pos().y());
	if (_client)
		_client->sendMousePosToServer(event->pos().x(), event->pos().y());
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
	if (_client)
		_client->sendMouseEventToServer(event->pos().x(), event->pos().y(), event->button() * _buttonMaskHashPress);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event)
{
	if (_client)
		_client->sendMouseEventToServer(event->pos().x(), event->pos().y(), event->button() * _buttonMaskHashRelease);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
	if (_client)
		_client->sendResizeToServer(event->size().width(), event->size().height());
}
