#include "StyleEditor.h"

#include <QApplication>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QMessageBox>
#include <QObject>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleFactory>
#include <set>

#include "Settings.h"

namespace StyleEditor {
const char *SYSTEM_STYLE = "<System>";

QString styleSheetDir(const QString &basedir, const QString &styleName) {
  return basedir + "/" + styleName;
}
QString styleSheetPath(const QString &basedir, const QString &styleName) {
  return styleSheetDir(basedir, styleName) + "/" + styleName + ".qss";
}

struct InvalidColorError {
  QString settingsKey;
  QString setting;
};
void setColorFromSetting(QPalette &palette, QPalette::ColorRole role,
                         QSettings &settings, const QString &key) {
  QString str = settings.value(key).toString();
  if (QColor::isValidColor(str))
    palette.setColor(role, str);
  else
    throw InvalidColorError{key, str};
};

void tryLoadPalette(const QString &basedir, const QString &styleName) {
  QString path = styleSheetDir(basedir, styleName) + "/palette.ini";
  if (QFile::exists(path)) {
    QPalette palette = qApp->palette();
    QSettings stylePalette(path, QSettings::IniFormat);

    stylePalette.beginGroup("palette");
    try {
      setColorFromSetting(palette, QPalette::Window, stylePalette, "Window");
      setColorFromSetting(palette, QPalette::WindowText, stylePalette,
                          "WindowText");
      setColorFromSetting(palette, QPalette::Base, stylePalette, "Base");
      setColorFromSetting(palette, QPalette::ToolTipBase, stylePalette,
                          "ToolTipBase");
      setColorFromSetting(palette, QPalette::ToolTipText, stylePalette,
                          "ToolTipText");
      setColorFromSetting(palette, QPalette::Text, stylePalette, "Text");
      setColorFromSetting(palette, QPalette::Button, stylePalette, "Button");
      setColorFromSetting(palette, QPalette::ButtonText, stylePalette,
                          "ButtonText");
      setColorFromSetting(palette, QPalette::BrightText, stylePalette,
                          "BrightText");
      setColorFromSetting(palette, QPalette::Text, stylePalette, "Text");
      setColorFromSetting(palette, QPalette::Link, stylePalette, "Link");
      setColorFromSetting(palette, QPalette::Highlight, stylePalette,
                          "Highlight");
      setColorFromSetting(palette, QPalette::Light, stylePalette, "Light");
      setColorFromSetting(palette, QPalette::Dark, stylePalette, "Dark");
      qApp->setPalette(palette);
    } catch (InvalidColorError e) {
      qWarning()
          << QString(
                 "Could not load palette. Invalid color (%1) for setting (%2)")
                 .arg(e.setting, e.settingsKey);
    }
  }
}

bool tryLoadStyle(const QString &basedir, const QString &styleName) {
  QFile styleSheet = styleSheetPath(basedir, styleName);
  if (!styleSheet.open(QFile::ReadOnly)) {
    qWarning() << "The selected style is not available: " << styleName << " ("
               << styleSheet.fileName() << ")";
    return false;
  }

  tryLoadPalette(basedir, styleName);
  // Only apply custom palette if palette.ini is present. For minimal
  // sheets, the availability of a palette helps their changes blend
  // in with unchanged aspects.
  qApp->setStyle(QStyleFactory::create("Fusion"));
  // Use Fusion as a base for aspects stylesheet does not cover, it should
  // look consistent across all platforms
  qApp->setStyleSheet(styleSheet.readAll());
  styleSheet.close();
  return true;
}

std::map<QString, QString> getStyleMap() {
  std::map<QString, QString> styles;
  styles[SYSTEM_STYLE] = "";

  QString exe_path = qApp->applicationDirPath();
  QStringList dirsToCheck =
      // Prioritize things in user config dir over application dir
      QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, "style",
                                QStandardPaths::LocateDirectory) +
      QStringList{exe_path + "/style", exe_path + "/../share/style"};

  for (const QString &basedir : dirsToCheck) {
    QDirIterator dir(basedir, QDirIterator::NoIteratorFlags);
    while (dir.hasNext()) {
      dir.next();

      if (dir.fileName().isEmpty() || dir.fileName() == "." ||
          dir.fileName() == "..")
        continue;

      QString styleName = dir.fileName();
      if (styles.count(styleName) > 0) continue;

      QString stylePath = styleSheetPath(basedir, styleName);
      if (!QFile(stylePath).exists()) continue;
      styles[styleName] = basedir;
    }
  }
  return styles;
}

bool tryLoadStyle(const QString &styleName) {
  if (styleName == SYSTEM_STYLE) return true;

  auto styles = getStyleMap();
  auto it = styles.find(styleName);

  if (it == styles.end()) {
    qWarning() << "No such available style: " << styleName;
    return false;
  }

  QString basedir = it->second;
  return tryLoadStyle(basedir, styleName);
}

QStringList getStyles() {
  QStringList styles;
  for (const auto &[style, dir] : getStyleMap()) styles.push_back(style);
  return styles;
}
}  // namespace StyleEditor
