#include "stdafx.h"
#include "OpenGLWidget.h"
#include <QRandomGenerator>
#include <QOpenGLFunctions_3_3_Core>

OpenGLWidget::OpenGLWidget(QWidget* parent)
	: QOpenGLWidget(parent)
	// using pixel data
	, _buffer(QOpenGLBuffer::PixelUnpackBuffer)
{
	QSurfaceFormat format;
	format.setVersion(3, 3);
	format.setProfile(QSurfaceFormat::CoreProfile);
	setFormat(format);
}

OpenGLWidget::~OpenGLWidget()
{
	makeCurrent();
	_buffer.destroy();
	doneCurrent();
}

void OpenGLWidget::updateFram(const QImage& image)
{
	if (!_initialized)
		return;
	if (image.isNull()) {
		qDebug() << "Received null image, skipping update.";
		return;
	}

	makeCurrent();

	if (!_texture)
	{
		_texture = std::make_shared<QOpenGLTexture>(image);
		_texture->create();

		_texture->setMinificationFilter(QOpenGLTexture::Linear);
		_texture->setMagnificationFilter(QOpenGLTexture::Linear);
		_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
	}
	_texture->bind();
	_texture->setData(image);
	_texture->release();

	doneCurrent();
	update();
}

void OpenGLWidget::initializeGL()
{
	initializeOpenGLFunctions();
	
	//_buffer.create();
	//_buffer.bind();
	//_buffer.allocate(1280 * 720 * 4);

	//// test data
 //   unsigned char* data = (unsigned char*)_buffer.map(QOpenGLBuffer::WriteOnly);
 //   for (int i = 0; i < 1280 * 720 * 4; i ++)
 //   {
	//	data[i] = static_cast<unsigned char>(QRandomGenerator::global()->bounded(256));
 //   }
 //   _buffer.unmap();
	//_buffer.release();


	//// texture
	//_texture = std::make_shared<QOpenGLTexture>(QOpenGLTexture::Target2D);
	//_texture->create();
	//_texture->bind();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 720, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//_texture->release();

	//// shader
	//_SP.addShaderFromSourceCode(QOpenGLShader::Vertex,
	//	"#version 330 core\n"
	//	"layout(location = 0) in vec2 texCoord;\n"
	//	"out vec2 v_TexCoord;\n"
	//	"void main() {\n"
	//	"   v_TexCoord = texCoord;\n"
	//	"   gl_Position = vec4(texCoord * 2.0 - 1.0, 0.0, 1.0);\n"
	//	"}"
	//);

	//_SP.addShaderFromSourceCode(QOpenGLShader::Fragment,
	//	"#version 330 core\n"
	//	"in vec2 v_TexCoord;\n"
	//	"out vec4 FragColor;\n"
	//	"uniform sampler2D textureSampler;\n"
	//	"void main() {\n"
	//	"   FragColor = texture(textureSampler, v_TexCoord);\n"
	//	"}"
	//);

	//_SP.link();


	//glClearColor(0.5f, 1.0f, 1.0f, 1.0f);




    static const char* vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec2 inPos;
    layout(location = 1) in vec2 inTexCoord;
    out vec2 texCoord;
    void main()
    {
        texCoord = inTexCoord;
        gl_Position = vec4(inPos, 0.0, 1.0);
    }
    )";

	static const char* fragmentShaderSource = R"(
		#version 330 core
		in vec2 texCoord;
	out vec4 fragColor;
	uniform sampler2D tex;
	void main()
	{
		fragColor = texture(tex, texCoord);
	}
		)";

	_SP.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
	_SP.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
	_SP.link();

	float vertices[] = {
		//   X      Y      U     V
		-1.0f, -1.0f,   0.0f,  1.0f,  // Bottom-left
		 1.0f, -1.0f,   1.0f,  1.0f,  // Bottom-right
		-1.0f,  1.0f,   0.0f,  0.0f,  // Top-left
		 1.0f,  1.0f,   1.0f,  0.0f   // Top-right
	};
	unsigned int indices[] = {
		0, 1, 2,
		1, 2, 3
	};
	_initialized = true;
}

void OpenGLWidget::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);
}

void OpenGLWidget::paintGL()
{
	glClear(GL_COLOR_BUFFER_BIT);

	_SP.bind();

	if (_texture)
	{
		_texture->bind(0);
		_SP.setUniformValue("tex", 0);
	}

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

	_SP.release();

	////_buffer.bind();
	////_texture->bind();
	////glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1280, 720, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	////_buffer.release();

	////_SP.bind();
	////glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	////_SP.release();

	//glBegin(GL_TRIANGLES);
	//glColor3f(1.0f, 0.0f, 0.0f);
	//glVertex3f(-0.6f, -0.6f, 0.0f);
	//glColor3f(0.0f, 1.0f, 0.0f);
	//glVertex3f(0.6f, -0.6f, 0.0f);
	//glColor3f(0.0f, 0.0f, 1.0f);
	//glVertex3f(0.0f, 0.6f, 0.0f);
	//glEnd();

}
