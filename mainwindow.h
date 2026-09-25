#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#ifndef APP_VERSION          // normally set from VERSION in GuitarPROtoTeX-Convert.pro
#define APP_VERSION "0.0.0-dev"
#endif
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QString>
#include <QDir>
#include <QCloseEvent>
#include <QDebug>
#include <QSettings>
#include <QActionGroup>
#include <QTranslator>
#include <QEvent>
#include <QPalette>

class LatexLexer;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    QString requestXMLFolderPath();
    QString requestTexFolderPath();
    QString p_xml;  // default path for MusicXML files
    QString p_tex;  // default path for TeX files

    /* --- Preferences (stored via QSettings, see readSettings()/writeSettings()) --- */
    void readSettings();                            // load all prefs (called once in the constructor)
    void writeSettings();                           // save all prefs immediately
    QString guiLanguage() const { return m_guiLanguage; }   // "en" or "de"
    QString guiTheme() const    { return m_guiTheme; }      // theme name, empty = system default style
    void setGuiLanguage(const QString &lang);       // store GUI language ("en"/"de") and save prefs
    void setGuiTheme(const QString &theme);         // store theme name, apply it and save prefs

    /* --- Themes (dark themes taken 1:1 from AmigaED 4.0) --- */
    bool isDarkTheme() const;                       // "Dark"
    bool isVSCodeTheme() const;                     // "Visual Studio Code Dark"
    bool isAnyDarkTheme() const { return isDarkTheme() || isVSCodeTheme(); }
    QString effectiveThemeName() const;             // m_guiTheme, or the system default style's key if empty

    /* --- GUI language (runtime switching) --- */
    static QString storedLanguage();                // language from prefs, fallback: system locale
    bool applyLanguage(const QString &lang);        // (un)install translators -> triggers LanguageChange

    /* --- File operations (public so they can be used without file dialogs) --- */
    bool loadMusicXmlFile(const QString &path);     // fill lineEditXML/lineEditLaTeX, then convert
    bool convertMusicXmlFile(const QString &path);  // convert and show the result in textEdit
    bool writeTexFile(const QString &path);         // write textEdit content as UTF-8
    static QString texNameFor(const QString &xmlPath); // "song.xml" -> "song.tex"

private slots:
    /* --- Menus --- */
    void on_actionQuit_triggered();                             // Quit this application with security request
    void on_actionNew_triggered();                              // empty all Widget's content
    void on_actionLoad_MusicXML_File_triggered();               // load a MusicXML file from its default path
    void on_actionSave_LaTeX_Snippet_triggered();               // save converted file (.tex) to its default path
    void on_actionAbout_triggered();                            // display Program informations in a QMessageBox
    void on_actionAbout_Qt_triggered();                         // display informations about Qt version
    void on_actionSave_As_triggered();                          // save LaTeX file with a new name (.tex)
    void on_actionCopy_to_Clipboard_triggered();                // copy contents of QScintilla Wirget to Clipboard
    void on_actionEnglish_triggered();                          // switch to GUI language: English (default) and save to prefs
    void on_actionDeutsch_triggered();                          // switch to GUI language: Deutsch and save to prefs
    void on_actionDefault_MusicXML_Files_Folder_triggered();    // select xml input default folder and save to prefs
    void on_actionDefault_LaTeX_Files_Folder_triggered();       // select LaTeX output default folder and save to prefs
    /* --- Buttons --- */
    void on_pushButtonXML_clicked();            // open a file dialog (Base from Prefs) and get MusicXML file for converting,
                                                // put result into lineEditXML,
                                                // then name converted output file (defaults to text from lineEditXML, stipped suffix + ".tex")
                                                // and put result into lineEditLaTeX
    void on_pushButtonNew_clicked();            // empty all Widget's content
    void on_pushButtonSave_clicked();           // same as File > Save LaTeX Snippet
    void on_pushButtonClipboard_clicked();

private:
    Ui::MainWindow *ui;
    bool myDebug = true;

    /* --- Preferences state --- */
    QString m_guiLanguage = QStringLiteral("en");   // GUI language, "en" (default) or "de"
    QString m_guiTheme;                             // selected theme, empty = system default
    QActionGroup *languageGroup = nullptr;          // makes English/Deutsch mutually exclusive

    void updateLanguageMenuChecks();                // tick the menu entry of the current language
    void updateFolderActionTips();                  // show current default folders in the status tips
    static QString startDirFor(const QString &path);// existing folder or home dir as dialog start

    /* --- Saving --- */
    QString m_savedTexPath;                         // last file written by Save/Save As
    bool saveLatex(bool alwaysAsk);                 // Save (alwaysAsk=false) / Save As (true)
    void resetAllContent();                         // empty all widgets (used by New, after the safety request)

    /* --- Translators (owned by MainWindow, installed on qApp) --- */
    QTranslator appTranslator;                      // our own strings (:/i18n/GuitarPROtoTeX-Convert_de.qm)
    QTranslator qtTranslator;                       // Qt's own strings (OK/Cancel, file dialogs, ...)
    bool m_uiReady = false;                         // true after setupUi(): retranslateUi() is safe

    /* --- QScintilla editor --- */
    LatexLexer *texLexer = nullptr;                 // LaTeX syntax highlighting of the output
    void setupEditor();                             // line numbers, font, lexer
    void updateLineNumberMarginWidth();             // grow margin with the number of lines

    /* --- Theme handling --- */
    QActionGroup *themeGroup = nullptr;             // entries of Prefs > Theme (exclusion enforced in code)
    QString m_systemStyleName;                      // style Qt chose at startup (= "system default")
    void buildThemeMenu();                          // native styles + separator + synthetic dark themes
    void syncThemeMenuCheckedState();               // checkmark on the current theme
    void applyTheme();                              // style + palette + stylesheet + editor colours
    void applyEditorColors();                       // margins, caret line, selection, lexer colours
    QPalette darkApplicationPalette() const;        // AmigaED "Dark"
    QPalette vscodeApplicationPalette() const;      // AmigaED "Visual Studio Code Dark"
    static QStringList syntheticThemes();           // {"Dark", "Visual Studio Code Dark"}

private slots:
    void onThemeActionTriggered();                  // a Prefs > Theme entry was clicked



protected:
    // Overriding the closeEvent to intercept the application exit
    void closeEvent(QCloseEvent *event) override;
    // Re-translate the whole GUI when the language changes at runtime
    void changeEvent(QEvent *event) override;
};
#endif // MAINWINDOW_H
