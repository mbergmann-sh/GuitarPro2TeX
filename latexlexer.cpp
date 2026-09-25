#include "latexlexer.h"

#include <Qsci/qsciscintilla.h>

LatexLexer::LatexLexer(QObject *parent)
    : QsciLexerCustom(parent)
{
}

QString LatexLexer::description(int style) const
{
    switch (style) {
    case Default:     return tr("Default");
    case Comment:     return tr("Comment");
    case Command:     return tr("Command");
    case Environment: return tr("Environment");
    case Brace:       return tr("Brace");
    case Math:        return tr("Math");
    case Number:      return tr("Number");
    case Special:     return tr("Special character");
    }
    return QString();   // empty = style does not exist (required by QsciLexer)
}

QColor LatexLexer::defaultColor(int style) const
{
    // Light colour set; the dark themes will override these later
    switch (style) {
    case Comment:     return QColor(0x00, 0x80, 0x00);  // green
    case Command:     return QColor(0x00, 0x00, 0xC0);  // blue
    case Environment: return QColor(0x8B, 0x00, 0x8B);  // magenta
    case Brace:       return QColor(0xA0, 0x00, 0x00);  // dark red
    case Math:        return QColor(0x00, 0x70, 0x70);  // teal
    case Number:      return QColor(0xB0, 0x50, 0x00);  // orange
    case Special:     return QColor(0x80, 0x80, 0x00);  // olive
    }
    return QColor(0x20, 0x20, 0x20);                    // default text
}

QFont LatexLexer::defaultFont(int style) const
{
    QFont f = m_baseFont.family().isEmpty() ? QsciLexerCustom::defaultFont(style) : m_baseFont;
    if (style == Comment)
        f.setItalic(true);
    return f;
}

void LatexLexer::setBaseFont(const QFont &font)
{
    m_baseFont = font;
    for (int s = Default; s <= Special; ++s)
        setFont(defaultFont(s), s);
}

static inline bool isLetter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '@';
}

void LatexLexer::styleText(int start, int end)
{
    QsciScintilla *ed = editor();
    if (!ed || start >= end)
        return;

    // Always restyle whole lines (the lexer state does not span lines)
    const int firstLine = int(ed->SendScintilla(QsciScintilla::SCI_LINEFROMPOSITION, start));
    start = int(ed->SendScintilla(QsciScintilla::SCI_POSITIONFROMLINE, firstLine));
    const int len = end - start;
    if (len <= 0)
        return;

    // Raw UTF-8 bytes of the range. All syntax characters are ASCII, so
    // multi-byte UTF-8 sequences (bytes >= 0x80) are simply styled as Default.
    QByteArray text(len + 1, '\0');
    ed->SendScintilla(QsciScintilla::SCI_GETTEXTRANGE, start, end, text.data());
    const char *t = text.constData();

    startStyling(start);
    int i = 0;
    auto emit_ = [&](int n, int style) { if (n > 0) { setStyling(n, style); i += n; } };

    while (i < len) {
        const char c = t[i];

        if (c == '%') {                                     // comment to end of line
            int j = i;
            while (j < len && t[j] != '\n') ++j;
            emit_(j - i, Comment);
        }
        else if (c == '\\') {                               // command
            int j = i + 1;
            if (j < len && isLetter(t[j])) {
                while (j < len && isLetter(t[j])) ++j;
                if (j < len && t[j] == '*') ++j;            // starred variant
            } else if (j < len && t[j] != '\n') {
                ++j;                                        // control symbol: \\ \% \{ ...
            }
            const QByteArray name(t + i, j - i);
            emit_(j - i, Command);

            // \begin{env} / \end{env}: colour the environment name
            if ((name == "\\begin" || name == "\\end") && i < len && t[i] == '{') {
                int k = i + 1;
                while (k < len && t[k] != '}' && t[k] != '\n') ++k;
                if (k < len && t[k] == '}') {
                    emit_(1, Brace);
                    emit_(k - i, Environment);
                    emit_(1, Brace);
                }
            }
        }
        else if (c == '$') {                                // inline/display math, same line
            const int open = (i + 1 < len && t[i + 1] == '$') ? 2 : 1;
            int j = i + open;
            while (j < len && t[j] != '\n') {
                if (t[j] == '\\') { j += 2; continue; }     // skip escaped chars like \$
                if (t[j] == '$') break;
                ++j;
            }
            if (j < len && t[j] == '$')
                j = qMin(len, j + open);                    // include closing $ or $$
            emit_(qMin(j, len) - i, Math);
        }
        else if (c == '{' || c == '}' || c == '[' || c == ']') {
            emit_(1, Brace);
        }
        else if (c >= '0' && c <= '9') {
            int j = i;
            while (j < len && t[j] >= '0' && t[j] <= '9') ++j;
            emit_(j - i, Number);
        }
        else if (c == '&' || c == '~' || c == '^' || c == '_' || c == '#') {
            emit_(1, Special);
        }
        else {
            int j = i + 1;                                  // run of plain text
            while (j < len) {
                const char d = t[j];
                if (d == '%' || d == '\\' || d == '$' || d == '{' || d == '}' || d == '['
                    || d == ']' || (d >= '0' && d <= '9') || d == '&' || d == '~'
                    || d == '^' || d == '_' || d == '#')
                    break;
                ++j;
            }
            emit_(j - i, Default);
        }
    }
}
