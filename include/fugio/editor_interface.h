#ifndef EDITOR_INTERFACE_H
#define EDITOR_INTERFACE_H

#include <QObject>
#include <QUuid>

#include <fugio/global.h>

class QMainWindow;

FUGIO_NAMESPACE_BEGIN
class EditInterface;
class EditorSignals;
FUGIO_NAMESPACE_END

FUGIO_NAMESPACE_BEGIN

#define IID_EDITOR		(QUuid("{ed673102-dcaa-4f38-b98e-6a7886f26a65}"))

typedef enum MenuId
{
	HELP
} MenuId;

class EditorInterface
{
public:
	virtual ~EditorInterface( void ) {}

	virtual fugio::EditorSignals *qobject( void ) = 0;

	virtual const fugio::EditorSignals *qobject( void ) const = 0;

	//-------------------------------------------------------------------------
	// Main Window

	virtual QMainWindow *mainWindow( void ) = 0;

	//-------------------------------------------------------------------------

	virtual void setEditTarget( fugio::EditInterface *pEditTarget ) = 0;

	//-------------------------------------------------------------------------
	// Adding menu entries to the editor application

	virtual void menuAddEntry( fugio::MenuId, QString pName, QObject *pObject, const char *pSlot ) = 0;

	virtual void menuAddFileImporter( QString pName ) = 0;
};

FUGIO_NAMESPACE_END

Q_DECLARE_INTERFACE( fugio::EditorInterface, "com.bigfug.fugio.editor/1.0" )

#endif // EDITOR_INTERFACE_H
