#include "SideMenu.h"

#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QList>
#include <QMessageBox>
#include <QSettings>

#include "IconHelper.h"
#include "editor/ComboOptions.h"
#include "editor/Settings.h"
#include "ui_SideMenu.h"

static QFileDialog* make_add_woice_dialog(QWidget* parent) {
  QFileDialog* dialog = new QFileDialog(
      parent, parent->tr("Select voice"),
      QSettings().value(WOICE_DIR_KEY).toString(),
      parent->tr("Instruments (*.ptvoice *.ptnoise *.wav *.ogg)"));

  QString dir(QSettings().value(WOICE_DIR_KEY).toString());
  if (!dir.isEmpty()) dialog->setDirectory(dir);
  return dialog;
}

SideMenu::SideMenu(UnitListModel* units, WoiceListModel* woices,
                   UserListModel* users, SelectWoiceDialog* add_unit_dialog,
                   DelayEffectModel* delays, OverdriveEffectModel* ovdrvs,
                   QWidget* parent)
    : QWidget(parent),
      ui(new Ui::SideMenu),
      m_add_woice_dialog(make_add_woice_dialog(this)),
      m_add_unit_dialog(add_unit_dialog),
      m_units(units),
      m_woices(woices),
      m_users(users),
      m_delays(delays),
      m_ovdrvs(ovdrvs) {
  m_add_woice_dialog->setFileMode(QFileDialog::ExistingFiles);
  ui->setupUi(this);
  for (auto [label, value] : quantizeXOptions)
    ui->quantX->addItem(label, value);
  for (auto [label, value] : quantizeYOptions)
    ui->quantY->addItem(label, value);
  for (auto [label, value] : paramOptions)
    ui->paramSelection->addItem(label, value);

  ui->unitList->setModel(m_units);
  ui->woiceList->setModel(m_woices);
  ui->usersList->setModel(m_users);
  ui->delayList->setModel(m_delays);
  ui->overdriveList->setModel(m_ovdrvs);
  ui->unitList->setItemDelegate(new UnitListDelegate);
  setPlay(false);
  for (auto* list : {ui->unitList, ui->woiceList, ui->delayList,
                     ui->overdriveList, ui->usersList}) {
    list->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    list->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
  }
  ui->usersList->horizontalHeader()->setSectionResizeMode(
      int(UserListModel::Column::Name), QHeaderView::Stretch);

  void (QComboBox::*comboItemActivated)(int) = &QComboBox::activated;

  connect(ui->quantX, comboItemActivated, this,
          &SideMenu::quantXIndexActivated);
  connect(ui->quantY, comboItemActivated, this,
          &SideMenu::quantYIndexActivated);
  connect(ui->playBtn, &QPushButton::clicked, this,
          &SideMenu::playButtonPressed);
  connect(ui->stopBtn, &QPushButton::clicked, this,
          &SideMenu::stopButtonPressed);
  connect(ui->unitList->selectionModel(),
          &QItemSelectionModel::currentRowChanged,
          [this](const QModelIndex& current, const QModelIndex& previous) {
            (void)previous;
            if (current.isValid()) emit currentUnitChanged(current.row());
          });
  connect(ui->unitList, &QTableView::clicked,
          [this](const QModelIndex& index) { emit unitClicked(index.row()); });
  connect(ui->saveBtn, &QPushButton::clicked, this,
          &SideMenu::saveButtonPressed);
  connect(ui->addUnitBtn, &QPushButton::clicked, m_add_unit_dialog,
          &QDialog::open);
  connect(ui->removeUnitBtn, &QPushButton::clicked, [this]() {
    QVariant data = ui->unitList->currentIndex()
                        .siblingAtColumn(int(UnitListColumn::Name))
                        .data();
    if (data.canConvert<QString>() &&
        QMessageBox::question(this, tr("Are you sure?"),
                              tr("Are you sure you want to delete the unit "
                                 "(%1)? This cannot be undone.")
                                  .arg(data.value<QString>())) ==
            QMessageBox::Yes)
      emit removeUnit();
  });
  connect(ui->followCheckbox, &QCheckBox::clicked, [this](bool is_checked) {
    emit followPlayheadClicked(is_checked ? FollowPlayhead::Jump
                                          : FollowPlayhead::None);
  });
  connect(ui->copyCheckbox, &QCheckBox::toggled, this, &SideMenu::copyChanged);

  connect(ui->addWoiceBtn, &QPushButton::clicked, [this]() {
    m_change_woice = false;
    m_add_woice_dialog->show();
  });
  connect(ui->changeWoiceBtn, &QPushButton::clicked, [this]() {
    if (ui->woiceList->currentIndex().row() >= 0) {
      m_change_woice = true;
      m_add_woice_dialog->show();
    }
  });
  connect(m_add_woice_dialog, &QFileDialog::currentChanged, this,
          &SideMenu::candidateWoiceSelected);

  connect(m_add_woice_dialog, &QDialog::accepted, this, [this]() {
    // Unfortunately, in the past when we set the directory after every click a
    // user reported crashes after changing directories multiple times.
    std::optional<int> change_woice_idx = std::nullopt;
    QString name;
    if (m_change_woice) {
      int idx = ui->woiceList->currentIndex().row();
      name = ui->woiceList->currentIndex()
                 .siblingAtColumn(int(WoiceListColumn::Name))
                 .data()
                 .toString();
      if (idx < 0 && ui->woiceList->model()->rowCount() > 0) return;
      change_woice_idx = idx;
    }
    for (const auto& filename : m_add_woice_dialog->selectedFiles())
      if (filename != "") {
        QSettings().setValue(WOICE_DIR_KEY, QFileInfo(filename).absolutePath());
        if (change_woice_idx == std::nullopt)
          emit addWoice(filename);
        else
          emit changeWoice(change_woice_idx.value(), filename);
      }
  });

  connect(ui->tempoField, &QLineEdit::editingFinished, [this]() {
    bool ok;
    int tempo = ui->tempoField->text().toInt(&ok);
    if (!ok || tempo < 20 || tempo > 600)
      QMessageBox::critical(this, tr("Invalid tempo"),
                            tr("Tempo must be number between 20 and 600"));
    else {
      ui->tempoField->setText(QString(tempo));
      emit tempoChanged(tempo);
    }
  });

  connect(ui->beatsField, &QLineEdit::editingFinished, [this]() {
    bool ok;
    int beats = ui->beatsField->text().toInt(&ok);
    if (!ok || beats < 1 || beats > 16)
      QMessageBox::critical(this, tr("Invalid beats"),
                            tr("Beats must be number between 1 and 16"));
    else {
      setBeats(beats);
      emit beatsChanged(beats);
    }
  });

  connect(ui->removeWoiceBtn, &QPushButton::clicked, [this]() {
    int idx = ui->woiceList->currentIndex().row();
    if (idx >= 0 && ui->woiceList->model()->rowCount() > 0) {
      QString name = ui->woiceList->currentIndex()
                         .siblingAtColumn(int(WoiceListColumn::Name))
                         .data()
                         .toString();
      if (QMessageBox::question(this, tr("Are you sure?"),
                                tr("Are you sure you want to delete the voice "
                                   "(%1)? This cannot be undone.")
                                    .arg(name)) == QMessageBox::Yes)
        emit removeWoice(idx);
    } else
      QMessageBox::warning(this, tr("Cannot remove voice"),
                           tr("Please select a valid voice to remove."));
  });

  /*connect(ui->woiceList->selectionModel(),
          &QItemSelectionModel::currentRowChanged,
          [this](const QModelIndex& curr, const QModelIndex&) {
            emit selectWoice(curr.row());
          });*/
  connect(ui->woiceList, &QTableView::clicked,
          [this](const QModelIndex& index) {
            if (index.column() == int(WoiceListColumn::Name) ||
                index.column() == int(WoiceListColumn::Key))
              emit selectWoice(index.row());
          });
  connect(m_add_unit_dialog, &QDialog::accepted, [this]() {
    QString name = m_add_unit_dialog->getUnitNameSelection();
    int idx = m_add_unit_dialog->getSelectedWoiceIndex();
    if (idx >= 0 && name != "")
      emit addUnit(idx, name);
    else
      QMessageBox::warning(this, "Invalid unit options",
                           "Name or selected instrument invalid");
  });

  connect(ui->paramSelection, comboItemActivated, this,
          &SideMenu::paramKindIndexActivated);
  connect(ui->addOverdriveBtn, &QPushButton::clicked, this,
          &SideMenu::addOverdrive);
  connect(ui->removeOverdriveBtn, &QPushButton::clicked, [this]() {
    int ovdrv_no = ui->overdriveList->selectionModel()->currentIndex().row();
    emit removeOverdrive(ovdrv_no);
  });
  connect(ui->upUnitBtn, &QPushButton::clicked,
          [this]() { emit moveUnit(true); });
  connect(ui->downUnitBtn, &QPushButton::clicked,
          [this]() { emit moveUnit(false); });
  {
    bool ok;
    int v;
    v = QSettings().value(VOLUME_KEY).toInt(&ok);
    if (ok) ui->volumeSlider->setValue(v);
  }
  connect(ui->volumeSlider, &QSlider::valueChanged, [this](int v) {
    QSettings().setValue(VOLUME_KEY, v);
    emit volumeChanged(v);
  });
  {
    bool ok;
    double v;
    v = QSettings()
            .value(BUFFER_LENGTH_KEY, DEFAULT_BUFFER_LENGTH)
            .toDouble(&ok);
    if (ok) ui->bufferLength->setText(QString("%1").arg(v, 0, 'f', 2));
  }
  connect(ui->bufferLength, &QLineEdit::editingFinished, [this]() {
    bool ok;
    double length = ui->bufferLength->text().toDouble(&ok);
    if (ok) {
      QSettings().setValue(BUFFER_LENGTH_KEY, length);
      ui->bufferLength->clearFocus();
      emit bufferLengthChanged(length);
    }
  });
  connect(ui->usersList, &QTableView::activated,
          [this](const QModelIndex& index) { emit userSelected(index.row()); });
  connect(ui->watchBtn, &QPushButton::clicked, [this]() {
    if (ui->usersList->selectionModel()->hasSelection())
      emit userSelected(ui->usersList->currentIndex().row());
  });
}

void SideMenu::setEditWidgetsEnabled(bool b) {
  // Really only this first one is necessary, since you can't add anything
  // else without it.
  ui->addWoiceBtn->setEnabled(b);
  ui->changeWoiceBtn->setEnabled(b);
  ui->removeWoiceBtn->setEnabled(b);
  ui->addUnitBtn->setEnabled(b);
  ui->removeUnitBtn->setEnabled(b);
  ui->tempoField->setEnabled(b);
  ui->beatsField->setEnabled(b);
  ui->upUnitBtn->setEnabled(b);
  ui->downUnitBtn->setEnabled(b);
}

void SideMenu::setTab(int index) { ui->tabWidget->setCurrentIndex(index); }

SideMenu::~SideMenu() { delete ui; }

void SideMenu::setQuantXIndex(int i) { ui->quantX->setCurrentIndex(i); }
void SideMenu::setQuantYIndex(int i) { ui->quantY->setCurrentIndex(i); }

void SideMenu::setModified(bool modified) {
  if (modified)
    ui->saveBtn->setText("*");
  else
    ui->saveBtn->setText("");
}

void SideMenu::setTempo(int tempo) {
  ui->tempoField->setText(QString("%1").arg(tempo));
}
void SideMenu::setBeats(int beats) {
  ui->beatsField->setText(QString("%1").arg(beats));
}

void SideMenu::setFollowPlayhead(FollowPlayhead follow) {
  ui->followCheckbox->setCheckState(follow == FollowPlayhead::None
                                        ? Qt::CheckState::Unchecked
                                        : Qt::CheckState::Checked);
}

void SideMenu::setCopy(bool copy) {
  ui->copyCheckbox->setCheckState(copy ? Qt::Checked : Qt::Unchecked);
}

void SideMenu::setParamKindIndex(int index) {
  if (ui->paramSelection->currentIndex() != index)
    ui->paramSelection->setCurrentIndex(index);
}

void SideMenu::setCurrentUnit(int u) { ui->unitList->selectRow(u); }
void SideMenu::setCurrentWoice(int u) { ui->woiceList->selectRow(u); }
void SideMenu::setPlay(bool playing) {
  if (playing) {
    ui->playBtn->setIcon(QIcon(":/icons/color/pause"));
  } else {
    ui->playBtn->setIcon(QIcon(":/icons/color/play"));
  }
  ui->stopBtn->setIcon(QIcon(":/icons/color/stop"));
  ui->saveBtn->setIcon(getIcon("save"));
  ui->upUnitBtn->setIcon(getIcon("up"));
  ui->downUnitBtn->setIcon(getIcon("down"));
}
