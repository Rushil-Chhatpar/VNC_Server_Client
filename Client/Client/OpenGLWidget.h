#pragma once

#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QtOpenGLWidgets/qopenglwidget.h>
#include <QtGui/qopengl.h>
#include <qopenglfunctions.h>
#include <qopenglbuffer.h>
#include <qopenglshaderprogram.h>
#include <qopengltexture.h>

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT

public:
	OpenGLWidget(QWidget* parent = nullptr);
	~OpenGLWidget();

	void updateFram(const QImage& image);

protected:
	void initializeGL() override;
	void resizeGL(int w, int h) override;
	void paintGL() override;

private:
	QOpenGLBuffer _buffer;
	QOpenGLShaderProgram _SP;
	std::shared_ptr<QOpenGLTexture> _texture;

	bool _initialized = false;
};

#endif // OPENGLWIDGET_H