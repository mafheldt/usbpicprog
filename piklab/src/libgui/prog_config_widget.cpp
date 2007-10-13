/***************************************************************************
 *   Copyright (C) 2005 Nicolas Hadacek <hadacek@kde.org>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#include "prog_config_widget.h"

#include <qlayout.h>
#include <qlabel.h>
#include <qwidgetstack.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qtimer.h>
#include <qtabwidget.h>
#include <kiconloader.h>

#include "global_config.h"
#include "port_selector.h"
#include "progs/list/prog_list.h"
#include "progs/base/prog_config.h"
#include "progs/gui/prog_config_widget.h"
#include "progs/gui/prog_group_ui.h"
#include "main_global.h"

//----------------------------------------------------------------------------
Programmer::OptionsConfigWidget::OptionsConfigWidget()
{
  QGridLayout *top = new QGridLayout(this, 1, 1, 10, 10);

  _powerDown = new QCheckBox(i18n("Power down target after programming."), this);
  top->addMultiCellWidget(_powerDown, 0,0, 0,1);

  _targetSelfPowered = new QCheckBox(i18n("Target is self-powered (when possible)."), this);
  top->addMultiCellWidget(_targetSelfPowered, 1,1, 0,1);

  _programAll = new QCheckBox(i18n("Program all memory even if not needed (slower)."), this);
  top->addMultiCellWidget(_programAll, 2,2, 0,1);

  _verifyProgrammed = new QCheckBox(i18n("Only verify programmed words in code memory (faster)."), this);
  top->addMultiCellWidget(_verifyProgrammed, 3,3, 0,1);

  _blankCheckAfterErase = new QCheckBox(i18n("Blank check after erase."), this);
  top->addMultiCellWidget(_blankCheckAfterErase, 4,4, 0,1);

  _preserveEeprom = new QCheckBox(i18n("Preserve data EEPROM when programming."), this);
  top->addMultiCellWidget(_preserveEeprom, 5,5, 0,1);

  _programEeprom = new QCheckBox(i18n("Program data EEPROM."), this);
  top->addMultiCellWidget(_programEeprom, 6,6, 0,1);

  top->setRowStretch(7, 1);
}

void Programmer::OptionsConfigWidget::loadConfig()
{
  Config config;
  _powerDown->setChecked(config.powerDownAfterProgramming());
  _targetSelfPowered->setChecked(config.isTargetSelfPowered());
  _programAll->setChecked(!config.onlyProgramNonMask());
  _verifyProgrammed->setChecked(config.onlyVerifyProgrammed());
  _blankCheckAfterErase->setChecked(config.blankCheckAfterErase());
  _preserveEeprom->setChecked(config.preserveEeprom());
  _programEeprom->setChecked(config.programEeprom());
}

QPixmap Programmer::OptionsConfigWidget::pixmap() const
{
  KIconLoader loader;
  return loader.loadIcon("piklab_config_programmer", KIcon::Toolbar, KIcon::SizeMedium);
}

void Programmer::OptionsConfigWidget::saveConfig()
{
  Config config;
  config.writePowerDownAfterProgramming(_powerDown->isChecked());
  config.writeTargetSelfPowered(_targetSelfPowered->isChecked());
  config.writeOnlyProgramNonMask(!_programAll->isChecked());
  config.writeOnlyVerifyProgrammed(_verifyProgrammed->isChecked());
  config.writeBlankCheckAfterErase(_blankCheckAfterErase->isChecked());
  config.writePreserveEeprom(_preserveEeprom->isChecked());
  config.writeProgramEeprom(_programEeprom->isChecked());
}

//----------------------------------------------------------------------------
Programmer::SelectConfigWidget::SelectConfigWidget()
{
  QGridLayout *top = new QGridLayout(this, 1, 1, 10, 10);

  // programmer type
  QLabel *label = new QLabel(i18n("Programmer in use:"), this);
  top->addWidget(label, 0, 0);
  _combo = new KeyComboBox(this);
  connect(_combo, SIGNAL(activated()), SLOT(programmerChanged()));
  top->addWidget(_combo, 0, 1);

  _tabWidget = new QTabWidget(this);
  top->addMultiCellWidget(_tabWidget, 1,1, 0,2);

  // port selector
  _portSelectorContainer = new Container;
  _portSelectorContainer->setMargin(10);
  _portSelector = new PortSelector(_portSelectorContainer);
  connect(_portSelector, SIGNAL(changed()), SLOT(portChanged()));
  _portSelectorContainer->addWidget(_portSelector, 0, 0);

  // specific programmer config
  _stack = new KeyWidgetStack;
  _stack->setMargin(10);
  KIconLoader loader;
  QPixmap icon = loader.loadIcon("configure", KIcon::Small);
  _tabWidget->addTab(_stack, icon, i18n("Specific"));
  Programmer::Lister::ConstIterator it;
  for (it=Programmer::lister().begin(); it!=Programmer::lister().end(); ++it) {
    _combo->appendItem(it.key(), it.data()->label());
    ::Programmer::ConfigWidget *pcw = static_cast<const ::Programmer::GroupUI *>(it.data()->gui())->createConfigWidget(_stack);
    pcw->loadConfig();
    _stack->appendItem(it.key(), pcw);
  }

  // init
  GlobalConfig config;
  _combo->setCurrentItem(config.programmerGroup().name());
  _stack->setCurrentItem(config.programmerGroup().name());
  QTimer::singleShot(0, this, SLOT(programmerChanged()));
}

void Programmer::SelectConfigWidget::portChanged()
{
  ::BusyCursor bc; // can take a few seconds to connect programmer...
  HardwareDescription hd;
  hd.port = _portSelector->portDescription();
  ::Hardware::Config *config = Main::programmerGroup().hardwareConfig();
  if (config) hd.name = config->currentHardware(hd.port.type);
  delete config;
  QWidget *w = _stack->item(_combo->currentItem());
  bool ok = static_cast< ::Programmer::ConfigWidget *>(w)->setPort(hd);
  _portSelector->setStatus(hd.port.type, ok ? i18n("Connection: Ok") : i18n("Connection: Error"));
}

QPixmap Programmer::SelectConfigWidget::pixmap() const
{
  KIconLoader loader;
  return loader.loadIcon("piklab_config_programmer", KIcon::Toolbar, KIcon::SizeMedium);
}

void Programmer::SelectConfigWidget::saveConfig()
{
  _portSelector->saveConfig();
  QString key = _combo->currentItem();
  static_cast<ConfigWidget *>(_stack->item(key))->saveConfig();
  GlobalConfig config;
  config.writeProgrammerGroup(*::Programmer::lister().group(key));
}

void Programmer::SelectConfigWidget::programmerChanged()
{
  QString key = _combo->currentItem();
  const ::Programmer::Group &group = *::Programmer::lister().group(key);
  bool isHardware = !group.isSoftware();
  bool hasTab = ( _tabWidget->indexOf(_portSelectorContainer)!=-1 );
  if (isHardware) {
    if ( !hasTab ) {
      KIconLoader loader;
      QPixmap icon = loader.loadIcon("connect_creating", KIcon::Small);
      _tabWidget->insertTab(_portSelectorContainer, icon, i18n("Port Selection"), 0);
    }
  } else if (hasTab) _tabWidget->removePage(_portSelectorContainer);
  _portSelector->setGroup(group);
  _stack->setCurrentItem(key);
  if (isHardware) QTimer::singleShot(0, this, SLOT(portChanged()));
}
