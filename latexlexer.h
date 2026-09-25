#ifndef LATEXLEXER_H
#define LATEXLEXER_H

#include <Qsci/qscilexercustom.h>
#include <QColor>
#include <QFont>

// Small custom LaTeX lexer for the QScintilla output editor.
// The stock QsciLexerTeX only colours TeX primitives (\begin, \end, ...) and
// has no comment style, so ordinary LaTeX/MusiXTeX commands stay black.
class LatexLexer : public QsciLexerCustom
{
    Q_OBJECT

public:
    enum Style {
        Default     = 0,    // plain text
        Comment     = 1,    // % ... end of line
        Command     = 2,    // \command, \\, \%, ...
        Environment = 3,    // name in \begin{name} / \end{name}
        Brace       = 4,    // { } [ ]
        Math        = 5,    // $...$ and $$...$$ (single line)
        Number      = 6,    // 0-9
        Special     = 7     // & ~ ^ _ #
    };

    explicit LatexLexer(QObject *parent = nullptr);

    const char *language() const override { return "LaTeX"; }
    QString description(int style) const override;
    QColor defaultColor(int style) const override;
    QFont defaultFont(int style) const override;
    void styleText(int start, int end) override;

    // Base font used for all styles (set from MainWindow::setupEditor())
    void setBaseFont(const QFont &font);

private:
    QFont m_baseFont;
};

#endif // LATEXLEXER_H
