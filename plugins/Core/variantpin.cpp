#include "variantpin.h"

#include <QPointF>
#include <QSizeF>

VariantPin::VariantPin( QSharedPointer<fugio::PinInterface> pPin )
	: PinControlBase( pPin )
{

}

QString VariantPin::toString() const
{
	switch( QMetaType::Type( mValue.type() ) )
	{
		case QMetaType::QPointF:
			{
				QPointF		V = mValue.value<QPointF>();

				return( QString( "%1,%2 (%3)" ).arg( V.x() ).arg( V.y() ) ).arg( QString( mValue.typeName() ) );
			}

		case QMetaType::QSizeF:
			{
				QSizeF		V = mValue.value<QSizeF>();

				return( QString( "%1,%2 (%3)" ).arg( V.width() ).arg( V.height() ) ).arg( QString( mValue.typeName() ) );
			}

		default:
			break;
	}

	return( QString( "%1 (%2)" ).arg( mValue.toString() ).arg( QString( mValue.typeName() ) ) );
}

