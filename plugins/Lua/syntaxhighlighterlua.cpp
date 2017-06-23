#include "syntaxhighlighterlua.h"

#include <QDebug>

SyntaxHighlighterLua::SyntaxHighlighterLua(QObject *parent) :
	QSyntaxHighlighter(parent)
{
}

SyntaxHighlighterLua::SyntaxHighlighterLua( QTextDocument *pDocument )
	: QSyntaxHighlighter( pDocument )
{
	HighlightingRule rule;

	keywordFormat.setForeground(Qt::darkBlue);
	keywordFormat.setFontWeight(QFont::Bold);
	QStringList keywordPatterns;
	keywordPatterns << "and"
					<< "break"
					<< "do"
					<< "else"
					<< "elseif"
					<< "end"
					<< "false"
					<< "for"
					<< "function"
					<< "if"
					<< "in"
					<< "local"
					<< "nil"
					<< "not"
					<< "or"
					<< "repeat"
					<< "return"
					<< "then"
					<< "true"
					<< "until"
					<< "while";

	foreach (const QString &pattern, keywordPatterns)
	{
		rule.pattern = QRegExp( QString( "\\b%1\\b" ).arg( pattern ) );
		rule.format = keywordFormat;
		highlightingRules.append(rule);
	}

	variableFormat.setForeground(Qt::darkRed);
	variableFormat.setFontWeight(QFont::Bold);

	QStringList variablePatterns;
//	variablePatterns << "and"
//					 << "break"
//					 << "while";

	foreach (const QString &pattern, variablePatterns)
	{
		rule.pattern = QRegExp( QString( "\\b%1\\b" ).arg( pattern ) );
		rule.format = variableFormat;
		highlightingRules.append(rule);
	}

	classFormat.setFontWeight(QFont::Bold);
	classFormat.setForeground(Qt::darkMagenta);
	rule.pattern = QRegExp("\\bQ[A-Za-z]+\\b");
	rule.format = classFormat;
	highlightingRules.append(rule);

	quotationFormat.setForeground(Qt::darkGreen);
	rule.pattern = QRegExp("\".*\"");
	rule.format = quotationFormat;
	highlightingRules.append(rule);

	functionFormat.setFontItalic(true);
	functionFormat.setForeground(Qt::blue);
	rule.pattern = QRegExp("\\b[A-Za-z0-9_]+(?=\\()");
	rule.format = functionFormat;
	highlightingRules.append(rule);

	singleLineCommentFormat.setForeground(Qt::red);
	rule.pattern = QRegExp("--[^\n]*");
	rule.format = singleLineCommentFormat;
	highlightingRules.append(rule);

	//	defineFormat.setForeground(Qt::darkMagenta);
	//	rule.pattern = QRegExp("#[^\n]*");
	//	rule.format = defineFormat;
	//highlightingRules.append(rule);

	multiLineCommentFormat.setForeground( Qt::red );

	commentStartExpression = QRegExp( "--\\[\\[" );
	commentEndExpression = QRegExp( "\\]\\]--" );
}

SyntaxHighlighterLua::~SyntaxHighlighterLua( void )
{
}

void SyntaxHighlighterLua::clearErrors()
{
	mErrorData.clear();

	emit errorsUpdated();

	rehighlight();
}

void SyntaxHighlighterLua::setErrors( const QString &pErrorText)
{
	QString			EL = pErrorText;

	mErrorData.clear();

	if( EL.startsWith( '"' ) )
	{
		EL.remove( 0, 1 );
	}

	if( EL.endsWith( '"' ) )
	{
		EL.chop( 1 );
	}

	QStringList		SL = EL.trimmed().split( '\n' );

	for( QString S : SL )
	{
		int				Line  = 0;

		S = S.trimmed();

		QRegExp	P( "^\\[string \"(.+)\"\\]:(\\d+):(.+)" );

		if( P.indexIn( S ) > -1 )
		{
			Line = P.cap( 2 ).toInt();

			if( Line > 0 )
			{
				fugio::SyntaxError	SE;

				SE.mLineStart   = SE.mLineEnd   = Line;
				SE.mColumnStart = SE.mColumnEnd = 0;

				SE.mError = P.cap( 3 );

				mErrorData << SE;
			}
		}
		else
		{
			qDebug() << S;
		}

		if( !Line )
		{
			fugio::SyntaxError	SE;

			SE.mLineStart   = SE.mLineEnd   = -1;
			SE.mColumnStart = SE.mColumnEnd = -1;

			SE.mError = S;

			mErrorData << SE;
		}
	}

	emit errorsUpdated();

	rehighlight();
}

QList<fugio::SyntaxError> SyntaxHighlighterLua::errorList( void ) const
{
	return( mErrorData );
}

void SyntaxHighlighterLua::highlightBlock( const QString &text )
{
	for( const HighlightingRule &rule : highlightingRules )
	{
		QRegExp expression( rule.pattern );

		int index = expression.indexIn(text);

		while( index >= 0 )
		{
			int length = expression.matchedLength();

			setFormat( index, length, rule.format );

			index = expression.indexIn(text, index + length);
		}
	}

	setCurrentBlockState( 0 );

	int startIndex = 0;

	if( previousBlockState() != 1 )
	{
		startIndex = commentStartExpression.indexIn(text);
	}

	while( startIndex >= 0 )
	{
		int endIndex = commentEndExpression.indexIn(text, startIndex);
		int commentLength;

		if (endIndex == -1)
		{
			setCurrentBlockState(1);

			commentLength = text.length() - startIndex;
		}
		else
		{
			commentLength = endIndex - startIndex + commentEndExpression.matchedLength();
		}

		setFormat(startIndex, commentLength, multiLineCommentFormat);

		startIndex = commentStartExpression.indexIn(text, startIndex + commentLength);
	}
}

