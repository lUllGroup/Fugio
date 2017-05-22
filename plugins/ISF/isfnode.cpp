#include "isfnode.h"

#include <QJsonDocument>
#include <QJsonArray>

#include <QByteArray>
#include <QPointF>
#include <QImage>
#include <QDateTime>

#include <fugio/core/uuid.h>
#include <fugio/colour/uuid.h>
#include <fugio/opengl/uuid.h>
#include <fugio/audio/uuid.h>

#include <fugio/colour/colour_interface.h>
#include <fugio/choice_interface.h>

#include <fugio/opengl/texture_interface.h>

#include "isfplugin.h"

ISFNode::ISFNode( QSharedPointer<fugio::NodeInterface> pNode )
	: NodeControlBase( pNode ), mVAO( 0 ), mBuffer( 0 ), mProgram( 0 ), mFrameCounter( 0 ), mUniformTime( -1 ),
	  mStartTime( -1 ), mLastRenderTime( 0 )
{
	FUGID( PIN_INPUT_SOURCE, "9e154e12-bcd8-4ead-95b1-5a59833bcf4e" );
	FUGID( PIN_OUTPUT_RENDER, "1b5e9ce8-acb9-478d-b84b-9288ab3c42f5" );

	mPinInputSource = pinInput( "Source", PIN_INPUT_SOURCE );

	mValOutputRender = pinOutput<fugio::RenderInterface *>( "Render", mPinOutputRender, PID_RENDER, PIN_OUTPUT_RENDER );
}

bool ISFNode::deinitialise()
{
	if( ISFPlugin::hasContextStatic() )
	{
		if( mVAO )
		{
			glDeleteVertexArrays( 1, &mVAO );
		}

		if( mBuffer )
		{
			glDeleteBuffers( 1, &mBuffer );
		}

		if( mProgram )
		{
			glDeleteProgram( mProgram );
		}

		QVector<GLuint>		TexLst;

		for( QMap<QString,ISFImport>::iterator it = mISFImports.begin() ; it != mISFImports.end() ; it++ )
		{
			GLuint		TexId = it.value().mTextureId;

			if( TexId )
			{
				TexLst << TexId;
			}
		}

		for( ISFPass &Pass : mISFPasses )
		{
			if( Pass.mTextureId )
			{
				TexLst << Pass.mTextureId;
			}
		}

		if( !TexLst.isEmpty() )
		{
			glDeleteTextures( TexLst.size(), TexLst.constData() );
		}
	}

	mVAO = 0;

	mBuffer = 0;

	mProgram = 0;

	return( NodeControlBase::deinitialise() );
}

ISFNode::ISFInputType ISFNode::isfType( QString Type )
{
	if( Type == "event" )		return( EVENT );
	if( Type == "bool" )		return( BOOL );
	if( Type == "long" )		return( LONG );
	if( Type == "float" )		return( FLOAT );
	if( Type == "point2D" )		return( POINT2D );
	if( Type == "image" )		return( IMAGE );
	if( Type == "color" )		return( COLOR );
	if( Type == "audio" )		return( AUDIO );
	if( Type == "audioFFT" )	return( AUDIOFFT );

	return( UNKNOWN );
}

QMap<QString,ISFNode::ISFInput> ISFNode::parseInputs( QJsonArray Inputs )
{
	QMap<QString,ISFInput>	ISFNewInputs;

	for( const QJsonValue v : Inputs )
	{
		const QJsonObject	Input = v.toObject();
		QString				Name  = Input.value( "NAME" ).toString();
		ISFInputType		Type = isfType( Input.value( "TYPE" ).toString() );

		if( !Name.isEmpty() )
		{
			QSharedPointer<fugio::PinInterface>	ISFPin = mNode->findInputPinByName( Name );

			ISFInput							CurISF = mISFInputs.value( Name );

			if( ISFPin && CurISF.mType != UNKNOWN && CurISF.mType != Type )
			{
				mNode->removePin( ISFPin );
			}

			CurISF.mType = Type;

			if( !ISFPin )
			{
				ISFPin = pinInput( Name, QUuid::createUuid() );
			}

			if( !ISFPin )
			{
				continue;
			}

			switch( Type )
			{
				case UNKNOWN:
				case EVENT:
					break;

				case BOOL:
					ISFPin->setValue( Input.value( "DEFAULT" ).toBool() );
					break;

				case LONG:
					{
						QJsonArray	Labels = Input.value( "LABELS" ).toArray();
						QJsonArray	Values = Input.value( "VALUES" ).toArray();

						QStringList		LabLst;
						QVariantList	ValLst;

						for( QJsonValueRef L : Labels )
						{
							LabLst << L.toString();
						}

						for( QJsonValueRef V : Values )
						{
							ValLst << V.toInt();
						}

						QSharedPointer<fugio::PinControlInterface>	PinInf = ISFPin->control();

						if( PinInf && ISFPin->controlUuid() != PID_CHOICE )
						{
							ISFPin->setControl( QSharedPointer<fugio::PinControlInterface>() );

							PinInf.clear();
						}

						if( !PinInf )
						{
							PinInf = mNode->context()->createPinControl( PID_CHOICE, ISFPin );

							if( PinInf )
							{
								ISFPin->setControl( PinInf );
							}
						}

						if( PinInf )
						{
							fugio::ChoiceInterface	*ChoiceInf = qobject_cast<fugio::ChoiceInterface *>( PinInf->qobject() );

							if( ChoiceInf )
							{
								ChoiceInf->setChoices( LabLst );

								ISFPin->setSetting( "VALUES", ValLst );
							}
						}

						int		DefVal = Input.value( "DEFAULT" ).toInt();

						if( DefVal < LabLst.size() )
						{
							ISFPin->setValue( LabLst.at( DefVal ) );
						}
					}
					break;

				case FLOAT:
					ISFPin->setValue( Input.value( "DEFAULT" ).toDouble() );
					break;

				case POINT2D:
					ISFPin->registerPinInputType( PID_POINT );

					ISFPin->setValue( QPointF() );
					break;

				case IMAGE:
					ISFPin->registerPinInputType( PID_OPENGL_TEXTURE );
					break;

				case COLOR:
					{
						ISFPin->registerPinInputType( PID_COLOUR );

						QColor		C;

						QJsonArray	DefaultColour = Input.value( "DEFAULT" ).toArray();

						if( !DefaultColour.isEmpty() )
						{
							if( DefaultColour.size() > 0 ) C.setRedF( DefaultColour.at( 0 ).toDouble() );
							if( DefaultColour.size() > 1 ) C.setGreenF( DefaultColour.at( 1 ).toDouble() );
							if( DefaultColour.size() > 2 ) C.setBlueF( DefaultColour.at( 2 ).toDouble() );
							if( DefaultColour.size() > 3 ) C.setAlphaF( DefaultColour.at( 3 ).toDouble() );
						}

						ISFPin->setValue( C );
					}
					break;

				case AUDIO:
					ISFPin->registerPinInputType( PID_AUDIO );
					break;

				case AUDIOFFT:
					ISFPin->registerPinInputType( PID_FFT );
					break;
			}

			ISFNewInputs[ Name ] = CurISF;
		}
	}

	return( ISFNewInputs );
}

QMap<QString, ISFNode::ISFImport> ISFNode::parseImports( const QDir &pDir, const QJsonObject Imports ) const
{
	QMap<QString,ISFImport>		NewImports;

	for( const QString Name : Imports.keys() )
	{
		QString				Path  = Imports.value( Name ).toObject().value( "PATH" ).toString();

		Path = pDir.absoluteFilePath( Path );

		if( mISFImports.contains( Name ) )
		{
			if( mISFImports.value( Name ).mPath == Path )
			{
				continue;
			}

			NewImports.insert( Name, mISFImports.value( Name ) );
		}

		ISFImport	Import;

		Import.mPath = Path;

		QImage		ImportImage( Path );

		if( ImportImage.isNull() )
		{
			qWarning() << "ISF cannot load IMPORT" << Path;

			continue;
		}

		glGenTextures( 1, &Import.mTextureId );

		if( Import.mTextureId )
		{
			glBindTexture( GL_TEXTURE_2D, Import.mTextureId );

			switch( ImportImage.format() )
			{
				case QImage::Format_ARGB32:
					glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, ImportImage.width(), ImportImage.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, ImportImage.constBits() );
					break;

				case QImage::Format_RGB888:
					glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, ImportImage.width(), ImportImage.height(), 0, GL_BGR, GL_UNSIGNED_BYTE, ImportImage.constBits() );
					break;

				case QImage::Format_RGBA8888:
					glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, ImportImage.width(), ImportImage.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, ImportImage.constBits() );
					break;
			}

			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

			glBindTexture( GL_TEXTURE_2D, 0 );
		}

		NewImports.insert( Name, Import );
	}

	return( NewImports );
}

QList<ISFNode::ISFPass> ISFNode::parsePasses( const QJsonArray pPasses ) const
{
	QList<ISFPass>		PassList;

	for( const QJsonValue v : pPasses )
	{
		const QJsonObject	PassObject = v.toObject();

		ISFPass				Pass;

		Pass.mTarget = PassObject.value( "TARGET" ).toString();
		Pass.mPersistent = PassObject.value( "PERSISTENT" ).toBool();
		Pass.mFloat = PassObject.value( "FLOAT" ).toBool();
		Pass.mWidth = PassObject.value( "WIDTH" ).toString();
		Pass.mHeight = PassObject.value( "HEIGHT" ).toString();

		PassList << Pass;
	}

	if( PassList.isEmpty() )
	{
		PassList << ISFPass();
	}

	return( PassList );
}

void ISFNode::parseISF( const QDir &pDir, const QByteArray pSource )
{
	int			CommentStart = pSource.indexOf( "/*" );
	int			CommentEnd   = pSource.indexOf( "*/", CommentStart );

	if( CommentStart == -1 || CommentEnd == -1 )
	{
		return;
	}

	QByteArray		Comment = pSource.mid( CommentStart + 2, CommentEnd - CommentStart - 2 );

	QJsonParseError	JERR;

	QJsonDocument	JSON = QJsonDocument::fromJson( Comment, &JERR );

	if( JSON.isNull() )
	{
		qWarning() << JERR.errorString();

		return;
	}

	const QJsonObject	JOBJ = JSON.object();

	if( !JOBJ.contains( "ISFVSN" ) || JOBJ.value( "ISFVSN" ).toString().split( '.' ).first() != "2" )
	{
		return;
	}

	// Process INPUTS

	QStringList				OldInputs = mISFInputs.keys();

	QJsonArray				Inputs = JOBJ.value( "INPUTS" ).toArray();

	QMap<QString,ISFInput>	NewInputs = parseInputs( Inputs );

	for( const QString &Name : NewInputs.keys() )
	{
		OldInputs.removeOne( Name );
	}

	for( QString Name : OldInputs )
	{
		QSharedPointer<fugio::PinInterface>	ISFPin = mNode->findInputPinByName( Name );

		if( ISFPin )
		{
			mNode->removePin( ISFPin );
		}
	}

	mISFInputs = NewInputs;

	// Process IMPORTED

	QJsonObject				Imports = JOBJ.value( "IMPORTED" ).toObject();

	QMap<QString,ISFImport>	NewImports = parseImports( pDir, Imports );

	QVector<GLuint>		TexLst;

	for( const QString &Name : mISFImports.keys() )
	{
		GLuint		OldTexId = mISFImports.value( Name ).mTextureId;

		if( NewImports.contains( Name ) && NewImports.value( Name ).mTextureId == OldTexId )
		{
			continue;
		}

		if( OldTexId )
		{
			TexLst << OldTexId;
		}
	}

	// Process PASSES

	QJsonArray				PassesArray = JOBJ.value( "PASSES" ).toArray();

	for( ISFPass &Pass : mISFPasses )
	{
		if( Pass.mTextureId )
		{
			TexLst << Pass.mTextureId;

			Pass.mTextureId = 0;
		}
	}

	mISFPasses = parsePasses( PassesArray );

	// Delete all unused textures

	if( !TexLst.isEmpty() )
	{
		glDeleteTextures( TexLst.size(), TexLst.constData() );
	}

	// Load the shader

	QString		ShaderSource = pSource.mid( CommentEnd + 2 );

	if( !loadShaders( ShaderSource ) )
	{
		return;
	}

	// Find all the uniform locations in the shader for our inputs and imports

	GLint		TextureIndex = 0;

	for( QMap<QString,ISFInput>::iterator it = mISFInputs.begin() ; it != mISFInputs.end() ; it++ )
	{
		ISFInput	&Inp = it.value();

		Inp.mUniform = glGetUniformLocation( mProgram, it.key().toLatin1() );

		if( Inp.mType == IMAGE )
		{
			Inp.mTextureIndex = TextureIndex++;
		}
	}

	for( QMap<QString,ISFImport>::iterator it = mISFImports.begin() ; it != mISFImports.end() ; it++ )
	{
		ISFImport	&Imp = it.value();

		Imp.mUniform = glGetUniformLocation( mProgram, it.key().toLatin1() );

		Imp.mTextureIndex = TextureIndex++;
	}

	for( ISFPass &Pass : mISFPasses )
	{
		if( !Pass.mTarget.isEmpty() )
		{
			Pass.mUniform = glGetUniformLocation( mProgram, Pass.mTarget.toLatin1() );

			Pass.mTextureIndex = TextureIndex++;
		}
	}

	mTextureIndexCount = TextureIndex;
}

void ISFNode::inputsUpdated( qint64 pTimeStamp )
{
	if( pTimeStamp && mPinInputSource->isUpdated( pTimeStamp ) )
	{
		QByteArray		Source = variant( mPinInputSource ).toByteArray();

		parseISF( QDir::current(), Source );
	}

	pinUpdated( mPinOutputRender );
}

bool ISFNode::loadShaders( const QString &pShaderSource )
{
	GLint	Result = GL_FALSE;
	GLint	InfoLogLength = 0;

	GLuint	VertexShader = glCreateShader( GL_VERTEX_SHADER );

	const QByteArray		VS =
			"#version 330 core\n"
			"layout( location = 0 ) in vec2 vertex;\n"
			"out vec2 isf_FragNormCoord;\n"
			"void isf_vertShaderInit( void )\n"
			"{\n"
			"   gl_Position = vec4( vertex, 0, 1 );\n"
			"	isf_FragNormCoord = ( vertex * 0.5 ) + 0.5;\n"
			"}\n"
			"\n"
			"void main(void)\n"
			"{\n"
			"	isf_vertShaderInit();"
			"}\n";

	const char			*VertexSourceArray = VS.constData();

	glShaderSource( VertexShader, 1, &VertexSourceArray, NULL );

	glCompileShader( VertexShader );

	glGetShaderiv( VertexShader, GL_COMPILE_STATUS, &Result );
	glGetShaderiv( VertexShader, GL_INFO_LOG_LENGTH, &InfoLogLength );

	if( Result == GL_FALSE )
	{
		QByteArray		InfoLogMessage;

		InfoLogMessage.resize( InfoLogLength );

		glGetShaderInfoLog( VertexShader, InfoLogLength, NULL, InfoLogMessage.data() );

		qWarning() << "ISF Vertex Shader:" << QString::fromLatin1( InfoLogMessage );

		return( false );
	}

	GLuint		FragmentShader = glCreateShader( GL_FRAGMENT_SHADER );

	QByteArray		FragmentSource =
			"#version 330 core\n"
			"#define gl_FragColor isf_OutputColour\n"
			"#define IMG_SIZE(i) textureSize(i,0)\n"
			"#define IMG_PIXEL(i,p) texelFetch(i,ivec2(p))\n"
			"#define IMG_NORM_PIXEL(i,p) texture(i,p)\n"
			"#define IMG_THIS_PIXEL(i) IMG_PIXEL(i,gl_FragCoord.xy)\n"
			"#define IMG_THIS_NORM_PIXEL(i) IMG_NORM_PIXEL(i,isf_FragNormCoord)\n"
			"uniform int PASSINDEX;\n"
			"uniform vec2 RENDERSIZE;\n"
			"uniform float TIME;\n"
			"uniform float TIMEDELTA;\n"
			"uniform vec4 DATE;\n"
			"uniform int FRAMEINDEX;\n"
			"in vec2 isf_FragNormCoord;\n"
			"out vec4 isf_OutputColour;\n";

	for( QMap<QString,ISFInput>::iterator it = mISFInputs.begin() ; it != mISFInputs.end() ; it++ )
	{
		switch( it.value().mType )
		{
			case EVENT:
			case BOOL:
				FragmentSource.append( QString( "uniform bool %1;\n" ).arg( it.key() ) );
				break;

			case LONG:
				FragmentSource.append( QString( "uniform int %1;\n" ).arg( it.key() ) );
				break;

			case FLOAT:
				FragmentSource.append( QString( "uniform float %1;\n" ).arg( it.key() ) );
				break;

			case POINT2D:
				FragmentSource.append( QString( "uniform vec2 %1;\n" ).arg( it.key() ) );
				break;

			case IMAGE:
				FragmentSource.append( QString( "uniform sampler2D %1;\n" ).arg( it.key() ) );
				break;

			case COLOR:
				FragmentSource.append( QString( "uniform vec4 %1;\n" ).arg( it.key() ) );
				break;

			case AUDIO:
				FragmentSource.append( QString( "uniform float %1;\n" ).arg( it.key() ) );
				break;

			case AUDIOFFT:
				FragmentSource.append( QString( "uniform float %1;\n" ).arg( it.key() ) );
				break;

			case UNKNOWN:
				break;
		}
	}

	for( QMap<QString,ISFImport>::iterator it = mISFImports.begin() ; it != mISFImports.end() ; it++ )
	{
		FragmentSource.append( QString( "uniform sampler2D %1;\n" ).arg( it.key() ) );
	}

	for( ISFPass &PassData : mISFPasses )
	{
		if( !PassData.mTarget.isEmpty() )
		{
			FragmentSource.append( QString( "uniform sampler2D %1;\n" ).arg( PassData.mTarget ) );
		}
	}

	FragmentSource.append( pShaderSource );

	qDebug() << FragmentSource.split( '\n' );

	const char			*FragmentSourceArray = FragmentSource.constData();

	glShaderSource( FragmentShader, 1, &FragmentSourceArray, NULL );

	glCompileShader( FragmentShader );

	glGetShaderiv( FragmentShader, GL_COMPILE_STATUS, &Result );
	glGetShaderiv( FragmentShader, GL_INFO_LOG_LENGTH, &InfoLogLength );

	if( Result == GL_FALSE )
	{
		QByteArray		InfoLogMessage;

		InfoLogMessage.resize( InfoLogLength );

		glGetShaderInfoLog( FragmentShader, InfoLogLength, NULL, InfoLogMessage.data() );

		qWarning() << "Fragment Shader:" << QString::fromLatin1( InfoLogMessage );

		glDeleteShader( VertexShader );

		return( false );
	}

	GLuint		Program = glCreateProgram();

	glAttachShader( Program, VertexShader );
	glAttachShader( Program, FragmentShader );

	glLinkProgram( Program );

	glGetShaderiv( Program, GL_LINK_STATUS, &Result );
	glGetShaderiv( Program, GL_INFO_LOG_LENGTH, &InfoLogLength );

	if( Result == GL_FALSE )
	{
		QByteArray		InfoLogMessage;

		InfoLogMessage.resize( InfoLogLength );

		glGetShaderInfoLog( Program, InfoLogLength, NULL, InfoLogMessage.data() );

		qWarning() << "Shader Link:" << QString::fromLatin1( InfoLogMessage );

		glDeleteShader( VertexShader );
		glDeleteShader( FragmentShader );

		return( false );
	}

	if( mProgram )
	{
		glDeleteProgram( mProgram );
	}

	mProgram = Program;

	mFrameCounter = 0;

	mStartTime = -1;

	mUniformTime = glGetUniformLocation( mProgram, "TIME" );
	mUniformDate = glGetUniformLocation( mProgram, "DATE" );
	mUniformRenderSize = glGetUniformLocation( mProgram, "RENDERSIZE" );
	mUniformPassIndex = glGetUniformLocation( mProgram, "PASSINDEX" );
	mUniformTimeDelta = glGetUniformLocation( mProgram, "TIMEDELTA" );
	mUniformFrameIndex = glGetUniformLocation( mProgram, "FRAMEINDEX" );

	return( true );
}

void ISFNode::renderInputs()
{
	for( QMap<QString,ISFInput>::iterator it = mISFInputs.begin() ; it != mISFInputs.end() ; it++ )
	{
		ISFInput		&InpDat = it.value();
		const GLint		 UniLoc = InpDat.mUniform;

		if( UniLoc == -1 )
		{
			continue;
		}

		QSharedPointer<fugio::PinInterface> P = mNode->findInputPinByName( it.key() );

		if( !P )
		{
			continue;
		}

		if( InpDat.mType == EVENT && InpDat.mEventFlag )
		{
			glUniform1i( UniLoc, false );

			InpDat.mEventFlag = false;
		}

		switch( InpDat.mType )
		{
			case UNKNOWN:
				break;

			case EVENT:
				glUniform1i( UniLoc, true );

				InpDat.mEventFlag = true;
				break;

			case BOOL:
				glUniform1i( UniLoc, variant( P ).toBool() );
				break;

			case LONG:
				glUniform1i( UniLoc, variant( P ).toInt() );
				break;

			case FLOAT:
				glUniform1f( UniLoc, variant( P ).toFloat() );
				break;

			case POINT2D:
				{
					QPointF	V = variant( P ).toPointF();

					glUniform2f( UniLoc, V.x(), V.y() );
				}
				break;

			case COLOR:
				{
					QColor	C = variant( P ).value<QColor>();

					glUniform4f( UniLoc, C.redF(), C.greenF(), C.blueF(), C.alphaF() );
				}
				break;

			case IMAGE:
				{
					fugio::OpenGLTextureInterface	*TexInf = input<fugio::OpenGLTextureInterface *>( P );

					if( TexInf )
					{
						glUniform1i( UniLoc, InpDat.mTextureIndex );

						glActiveTexture( GL_TEXTURE0 + InpDat.mTextureIndex );

						TexInf->srcBind();
					}
				}
				break;
		}
	}
}

void ISFNode::renderImports()
{
	for( QMap<QString,ISFImport>::iterator it = mISFImports.begin() ; it != mISFImports.end() ; it++ )
	{
		ISFImport		&ImpDat = it.value();

		if( !ImpDat.mTextureId || ImpDat.mUniform == -1 )
		{
			continue;
		}

		glUniform1i( ImpDat.mUniform, ImpDat.mTextureIndex );

		glActiveTexture( GL_TEXTURE0 + ImpDat.mTextureIndex );

		glBindTexture( GL_TEXTURE_2D, ImpDat.mTextureId );
	}
}

void ISFNode::renderPasses( GLint Viewport[ 4 ] )
{
	for( ISFPass &PassData : mISFPasses )
	{
		if( PassData.mTarget.isEmpty() )
		{
			continue;
		}

		if( !PassData.mFBO )
		{
			glGenFramebuffers( 1, &PassData.mFBO );
		}

		if( !PassData.mFBO )
		{
			continue;
		}

		QSize		NewSze( Viewport[ 2 ], Viewport[ 3 ] );

		// TODO: calculate new size here

		NewSze.setWidth( NewSze.width() / 16 );
		NewSze.setHeight( NewSze.height() / 16 );

		if( NewSze != PassData.mSize )
		{
			if( PassData.mTextureId )
			{
				glBindFramebuffer( GL_FRAMEBUFFER, PassData.mFBO );

				glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0 );

				glBindFramebuffer( GL_FRAMEBUFFER, 0 );

				glDeleteTextures( 1, &PassData.mTextureId );

				PassData.mTextureId = 0;
			}

			PassData.mSize = NewSze;
		}

		if( PassData.mTextureId )
		{
			continue;
		}

		// Allocate a new texture

		glGenTextures( 1, &PassData.mTextureId );

		if( PassData.mTextureId )
		{
			glBindTexture( GL_TEXTURE_2D, PassData.mTextureId );

			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, PassData.mSize.width(), PassData.mSize.height(), 0, GL_RGBA, PassData.mFloat ? GL_FLOAT : GL_UNSIGNED_BYTE, NULL );

			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		}

		if( !PassData.mTextureId )
		{
			continue;
		}

		// Set up our FBO

		glBindFramebuffer( GL_FRAMEBUFFER, PassData.mFBO );

		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, PassData.mTextureId, 0 );

		GLenum DrawBuffers[ 1 ] = { GL_COLOR_ATTACHMENT0 };

		glDrawBuffers( 1, DrawBuffers );

		if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		{
			qWarning() << "ISF GL_FRAMEBUFFER != GL_FRAMEBUFFER_COMPLETE";
		}

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}
}

void ISFNode::renderUniforms( qint64 pTimeStamp, GLint Viewport[ 4 ] )
{
	if( mUniformRenderSize != -1 )
	{
		glUniform2f( mUniformRenderSize, Viewport[ 2 ], Viewport[ 3 ] );
	}

	if( mUniformTime != -1 )
	{
		glUniform1f( mUniformTime, GLfloat( pTimeStamp - mStartTime ) / 1000.0f );
	}

	if( mUniformTimeDelta != -1 )
	{
		glUniform1f( mUniformTimeDelta, GLfloat( pTimeStamp - mLastRenderTime ) / 1000.0f );
	}

	if( mUniformFrameIndex != -1 )
	{
		glUniform1i( mUniformFrameIndex, mFrameCounter );
	}

	if( mUniformDate != -1 )
	{
		const QDateTime		DateTime = QDateTime::currentDateTime();

		glUniform4f( mUniformDate, DateTime.date().year(), DateTime.date().month(), DateTime.date().day(), DateTime.time().msecsSinceStartOfDay() / 1000 );
	}
}

void ISFNode::render( qint64 pTimeStamp, QUuid pSourcePinId )
{
	Q_UNUSED( pSourcePinId )

	if( !ISFPlugin::hasContextStatic() )
	{
		return;
	}

	GLint		Viewport[ 4 ];

	glGetIntegerv( GL_VIEWPORT, Viewport );

	if( mStartTime == -1 )
	{
		mStartTime = pTimeStamp;
	}

	if( !mVAO && GLEW_ARB_vertex_array_object )
	{
		glGenVertexArrays( 1, &mVAO );
	}

	if( mVAO )
	{
		glBindVertexArray( mVAO );
	}

	if( !mBuffer && GLEW_VERSION_1_5 )
	{
		GLfloat		Verticies[][ 2 ] = {
			{ -1,  1 },
			{ -1, -1 },
			{  1,  1 },
			{  1, -1 }
		};

		glGenBuffers( 1, &mBuffer );
		glBindBuffer( GL_ARRAY_BUFFER, mBuffer );
		glBufferData( GL_ARRAY_BUFFER, sizeof( Verticies ), Verticies, GL_STATIC_DRAW );
	}

	if( mProgram )
	{
		glClearColor( 0.0f, 0.0f, 0.4f, 0.0f );

		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		glEnableVertexAttribArray( 0 );

		glBindBuffer( GL_ARRAY_BUFFER, mBuffer );

		glVertexAttribPointer(
		   0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		   2,                  // size
		   GL_FLOAT,           // type
		   GL_FALSE,           // normalized?
		   0,                  // stride
		   (void*)0            // array buffer offset
		);

		glUseProgram( mProgram );

		//---------------------------------------------------------------------
		// Set the INPUTS uniforms

		renderInputs();

		//---------------------------------------------------------------------
		// Set up the IMPORTED textures

		renderImports();

		//---------------------------------------------------------------------
		// Set predefined uniforms

		renderUniforms( pTimeStamp, Viewport );

		//---------------------------------------------------------------------
		// Updated passes

		renderPasses( Viewport );

		//---------------------------------------------------------------------
		// Bind PASSES render targets

		for( ISFPass &PassData : mISFPasses )
		{
			if( PassData.mUniform != -1 )
			{
				glUniform1i( PassData.mUniform, PassData.mTextureIndex );
			}

			glActiveTexture( GL_TEXTURE0 + PassData.mTextureIndex );

			glBindTexture( GL_TEXTURE_2D, PassData.mTextureId );
		}

		//---------------------------------------------------------------------
		// Perform rendering passes

		for( int Pass = 0 ; Pass < mISFPasses.size() ; Pass++ )
		{
			if( mUniformPassIndex != -1 )
			{
				glUniform1i( mUniformPassIndex, Pass );
			}

			ISFPass		&PassData = mISFPasses[ Pass ];

			// we can't read and write to textures at the same time

			if( PassData.mTextureId )
			{
				glActiveTexture( GL_TEXTURE0 + PassData.mTextureIndex );

				glBindTexture( GL_TEXTURE_2D, 0 );
			}

			if( PassData.mFBO )
			{
				glBindFramebuffer( GL_FRAMEBUFFER, PassData.mFBO );

				glViewport( 0, 0, PassData.mSize.width(), PassData.mSize.height() );
			}

			glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

			if( PassData.mFBO )
			{
				glBindFramebuffer( GL_FRAMEBUFFER, 0 );

				glViewport( Viewport[ 0 ], Viewport[ 1 ], Viewport[ 2 ], Viewport[ 3 ] );
			}

			if( PassData.mTextureId )
			{
				glActiveTexture( GL_TEXTURE0 + PassData.mTextureIndex );

				glBindTexture( GL_TEXTURE_2D, PassData.mTextureId );
			}
		}

		//---------------------------------------------------------------------
		// Clean-up

		glDisableVertexAttribArray( 0 );

		glUseProgram( 0 );

		// release all textures

		for( GLuint i = 0 ; i < mTextureIndexCount ; i++ )
		{
			glActiveTexture( GL_TEXTURE0 + i );

			glBindTexture( GL_TEXTURE_2D, 0 );
		}

		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

		mLastRenderTime = pTimeStamp;

		mFrameCounter++;
	}

	if( mVAO )
	{
		glBindVertexArray( 0 );
	}
}


