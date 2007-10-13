/***************************************************************************
 *   Copyright (C) 2005 Nicolas Hadacek <hadacek@kde.org>                  *
 *   Copyright (C) 2003 Alain Gibaud <alain.gibaud@free.fr>                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#ifndef PROJECT_H
#define PROJECT_H

#include <qdom.h>

#include "common/global/purl.h"
#include "devices/base/register.h"

//----------------------------------------------------------------------------
class XmlDataFile
{
public:
  XmlDataFile(const PURL::Url &url, const QString &name);
  virtual ~XmlDataFile() {}
  PURL::Url url() const { return _url; }
  virtual bool load(QString &error);
  bool save(QString &error) const;

  QString value(const QString &group, const QString &key, const QString &defaultValue) const;
  void setValue(const QString &group, const QString &key, const QString &value);
  QStringList listValues(const QString &group, const QString &key, const QStringList &defaultValues) const;
  void setListValues(const QString &group, const QString &key, const QStringList &values);
  void appendListValue(const QString &group, const QString &key, const QString &value);
  void removeListValue(const QString &group, const QString &key, const QString &value);
  void clearList(const QString &group, const QString &key);

protected:
  PURL::Url _url;

private:
  QString      _name;
  QDomDocument _document;

  QDomElement findChildElement(QDomElement element, const QString &tag) const;
  QDomElement createChildElement(QDomElement element, const QString &tag);
  void removeChilds(QDomNode node) const;
};

//----------------------------------------------------------------------------
class Project : public XmlDataFile
{
public:
  Project(const PURL::Url &url) : XmlDataFile(url, "piklab") {}
  virtual bool load(QString &error);

  PURL::Directory directory() const { return url().directory(); }
  QString name() const { return url().basename(); }
  PURL::UrlList absoluteFiles() const;
  QString version() const;
  QString description() const;
  PURL::UrlList openedFiles() const;
  PURL::Url customLinkerScript() const;
  QValueList<Register::TypeData> watchedRegisters() const;
  QString toSourceObject(const PURL::Url &url, const QString &extension, bool forWindows) const;
  QStringList objectsForLinker(const QString &extension, bool forWindows) const;
  QStringList librariesForLinker(const QString &prefix, bool forWindows) const;

  void removeFile(const PURL::Url &url); // take absolute filepath (but inside project dir)
  void addFile(const PURL::Url &url); // take absolute filePath (but inside project dir)
  void clearFiles();
  void setVersion(const QString &version);
  void setDescription(const QString &description);
  void setOpenedFiles(const PURL::UrlList &list);
  void setCustomLinkerScript(const PURL::Url &url);
  void setWatchedRegisters(const QValueList<Register::TypeData> &watched);
};

#endif
