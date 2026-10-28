#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QApplication>
#include <QWidget>
#include <QString>
#include <QColor>
#include <QPalette>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum ThemeType {
        DARK_THEME,
        LIGHT_THEME,
        FAMILIAR_THEME
    };

    struct ThemeColors {
        QColor background;
        QColor surface;
        QColor primary;
        QColor secondary;
        QColor textPrimary;
        QColor border;
        QColor hover;
    };

    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager();

    void setTheme(ThemeType theme);
    ThemeType getCurrentTheme() const;
    QString getStyleSheet() const;
    const ThemeColors& getCurrentColors() const;

private:
    ThemeType m_currentTheme;
    ThemeColors m_currentColors;
    
    void updateCurrentColors();
    void applyThemeToApplication();
    QString generateMainStyleSheet() const;
};

#endif // THEMEMANAGER_H