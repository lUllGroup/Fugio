#include "geometryplugin.h"

#include <QtPlugin>

#include <QDebug>

#include <QTranslator>
#include <QApplication>

#include <fugio/geometry/uuid.h>
#include <fugio/nodecontrolbase.h>

#include "polygonnode.h"

QList<QUuid>				NodeControlBase::PID_UUID;

ClassEntry		GeometryPlugin::mNodeClasses[] =
{
	ClassEntry( "Polygon", "Geometry", NID_POLYGON, &PolygonNode::staticMetaObject ),
	ClassEntry()
};

ClassEntry		GeometryPlugin::mPinClasses[] =
{
	ClassEntry()
};

GeometryPlugin::GeometryPlugin( void )
	: mApp( 0 )
{
	//-------------------------------------------------------------------------
	// Install translator

	static QTranslator		Translator;

	if( Translator.load( QLocale(), QLatin1String( "fugio_geometry" ), QLatin1String( "_" ), ":/translations" ) )
	{
		qApp->installTranslator( &Translator );
	}
}

GeometryPlugin::~GeometryPlugin( void )
{
}

PluginInterface::InitResult GeometryPlugin::initialise( fugio::GlobalInterface *pApp, bool pLastChance )
{
	Q_UNUSED( pLastChance )

	mApp = pApp;

	mApp->registerNodeClasses( mNodeClasses );

	mApp->registerPinClasses( mPinClasses );

	return( INIT_OK );
}

void GeometryPlugin::deinitialise()
{
	mApp->unregisterPinClasses( mPinClasses );

	mApp->unregisterNodeClasses( mNodeClasses );

	mApp = 0;
}
