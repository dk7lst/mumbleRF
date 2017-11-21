/* Copyright (C) 2005-2011, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "mumble_pch.hpp"

#include "GlobalShortcut.h"

#include "AudioInput.h"
#include "ClientUser.h"
#include "Channel.h"
#include "Database.h"
#include "Global.h"
#include "MainWindow.h"

/*!
  \class ShortcutKeyWidget
  Widget used to define and key combination for a shortcut. Once it gains
  focus it will listen for a button combination until it looses focus.
*/

/*!
  \class ShortcutActionWidget
  Combo box widget used to define the kind of action a shortcut triggers. Then
  entries get auto-generated from the GlobalShortcutEngine::qmShortcuts store.

  \see GlobalShortcutEngine
*/

/*!
  \class ShortcutTargetDialog
  Dialog which is used to select the targets of a targeted shortcut like Whisper.
*/

/*!
  \class ShortcutTargetWidget
  Widget used to display and change a ShortcutTarget. The widget displays a textual representation
  of a ShortcutTarget and enable its editing with a ShortCutTargetDialog.
*/

/*!
  \fn QString ShortcutTargetWidget::targetString(const ShortcutTarget &st)
  This function returns a textual representation of the given shortcut target \a st.
*/

/*!
  \class ShortcutDelegate
  Used to get custom display and edit behaviour for the model used in GlobalShortcutConfig::qtwShortcuts.
  It registers custom handlers which link specific types to custom ShortcutXWidget editors and also
  provides a basic textual representation for them when they are not edited.

  \see GlobalShortcutConfig
  \see ShortcutKeyWidget
  \see ShortcutActionWidget
  \see ShortcutTargetWidget
*/

/*!
  \fn QString ShortcutDelegate::displayText(const QVariant &item, const QLocale &loc)
  Provides textual representations for the mappings done for the edit behaviour.
*/

/*!
  \class GlobalShortcutConfig
  Contains the \a Shortcut tab from the settings.  This ConfigWidget provides
  the user with the interface to add/edit/delete global shortcuts in Mumble.
*/

/*!
  \class GlobalShortcutEngine
  Creates a background thread which handles global shortcut behaviour. This class inherits
  a system unspecific interface and basic functionality to the actually used native backend
  classes (GlobalShortcutPlatform).

  \see GlobalShortcutX
  \see GlobalShortcutMac
  \see GlobalShortcutWin
*/

/*!
  \fn static GlobalShortcutEngine *GlobalShortcutEngine::platformInit()
  Returns a platform specific GlobalShortcutEngine object.

  \see GlobalShortcutX
  \see GlobalShortcutMac
  \see GlobalShortcutWin
*/

/*!
  \var static GlobalShortcutEngine *GlobalShortcutEngine::engine
  Used to save the global, unique, platform specific GlobalShortcutEngine.
*/

/*!
  \fn bool GlobalShortcutEngine::handleButton(const QVariant &button, bool down)

  This function gets called internally to update the state
  of a \a button.

  \return True if button is suppressed, otherwise false
*/


GlobalShortcutEngine *GlobalShortcutEngine::engine = NULL;

static ConfigWidget *GlobalShortcutConfigDialogNew(Settings &st) {
	return new GlobalShortcutConfig(st);
}

static ConfigRegistrar registrar(1200, GlobalShortcutConfigDialogNew);

static const QString UPARROW = QString::fromUtf8("\xE2\x86\x91 ");

ShortcutKeyWidget::ShortcutKeyWidget(QWidget *p) : QLineEdit(p) {
	setReadOnly(true);
	clearFocus();
	bModified = false;
	displayKeys();
}

QList<QVariant> ShortcutKeyWidget::getShortcut() const {
	return qlButtons;
}

void ShortcutKeyWidget::setShortcut(const QList<QVariant> &buttons) {
	qlButtons = buttons;
	displayKeys();
}

void ShortcutKeyWidget::focusInEvent(QFocusEvent *) {
	setText(tr("Press Shortcut"));

	QPalette pal=parentWidget()->palette();
	pal.setColor(QPalette::Base, pal.color(QPalette::Base).dark(120));
	setPalette(pal);

	setForegroundRole(QPalette::Button);
	GlobalShortcutEngine::engine->resetMap();
	connect(GlobalShortcutEngine::engine, SIGNAL(buttonPressed(bool)), this, SLOT(updateKeys(bool)));
	installEventFilter(this);
}

void ShortcutKeyWidget::focusOutEvent(QFocusEvent *e) {
	if ((e->reason() == Qt::TabFocusReason) || (e->reason() == Qt::BacktabFocusReason))
		return;

	setPalette(parentWidget()->palette());
	clearFocus();
	disconnect(GlobalShortcutEngine::engine, SIGNAL(buttonPressed(bool)), this, SLOT(updateKeys(bool)));
	displayKeys();
	removeEventFilter(this);
}

bool ShortcutKeyWidget::eventFilter(QObject *, QEvent *evt) {
	if ((evt->type() == QEvent::KeyPress) || (evt->type() == QEvent::MouseButtonPress))
		return true;
	return false;
}

void ShortcutKeyWidget::mouseDoubleClickEvent(QMouseEvent *) {
	bModified = true;
	qlButtons.clear();
	clearFocus();
	displayKeys();
}

void ShortcutKeyWidget::updateKeys(bool last) {
	qlButtons = GlobalShortcutEngine::engine->qlActiveButtons;
	bModified = true;

	if (qlButtons.isEmpty())
		return;

	if (last)
		clearFocus();
	else
		displayKeys(false);
}

void ShortcutKeyWidget::displayKeys(bool last) {
	QStringList keys;

	foreach(QVariant button, qlButtons) {
		QString id = GlobalShortcutEngine::engine->buttonName(button);
		if (! id.isEmpty())
			keys << id;
	}
	setText(keys.join(QLatin1String(" + ")));
	emit keySet(keys.count() > 0, last);
}

ShortcutActionWidget::ShortcutActionWidget(QWidget *p) : QComboBox(p) {
	int idx = 0;

	insertItem(idx, tr("Unassigned"));
	setItemData(idx, -1);
#ifndef Q_OS_MAC
	setSizeAdjustPolicy(AdjustToContents);
#endif

	idx++;

	bool expert = true;
	// Try to traverse to GlobalShortcutConfig to get expert state (default: true)
	while (p) {
		GlobalShortcutConfig *gsc = qobject_cast<GlobalShortcutConfig *>(p);
		if (gsc) {
			expert = gsc->bExpert;
			break;
		}
		p = p->parentWidget();
	}
	foreach(GlobalShortcut *gs, GlobalShortcutEngine::engine->qmShortcuts) {
		// Hide all expert actions if we are not in expert mode
		if (expert || ! gs->bExpert) {
			insertItem(idx, gs->name);
			setItemData(idx, gs->idx);
			if (! gs->qsToolTip.isEmpty())
				setItemData(idx, gs->qsToolTip, Qt::ToolTipRole);
			if (! gs->qsWhatsThis.isEmpty())
				setItemData(idx, gs->qsWhatsThis, Qt::WhatsThisRole);
			idx++;
		}
	}
}

void ShortcutActionWidget::setIndex(int idx) {
	setCurrentIndex(findData(idx));
}

unsigned int ShortcutActionWidget::index() const {
	return itemData(currentIndex()).toUInt();
}

ShortcutToggleWidget::ShortcutToggleWidget(QWidget *p) : QComboBox(p) {
	int idx = 0;

	insertItem(idx, tr("Off"));
	setItemData(idx, -1);
	idx++;

	insertItem(idx, tr("Toggle"));
	setItemData(idx, 0);
	idx++;

	insertItem(idx, tr("On"));
	setItemData(idx, 1);
	idx++;
}

void ShortcutToggleWidget::setIndex(int idx) {
	setCurrentIndex(findData(idx));
}

int ShortcutToggleWidget::index() const {
	return itemData(currentIndex()).toInt();
}

void iterateChannelChildren(QTreeWidgetItem *root, Channel *chan, QMap<int, QTreeWidgetItem *> &map) {
	foreach(Channel *c, chan->qlChannels) {
		QTreeWidgetItem *sub = new QTreeWidgetItem(root, QStringList(c->qsName));
		sub->setData(0, Qt::UserRole, c->iId);
		map.insert(c->iId, sub);
		iterateChannelChildren(sub, c, map);
	}
}

ShortcutTargetDialog::ShortcutTargetDialog(const ShortcutTarget &st, QWidget *pw) : QDialog(pw) {
	stTarget = st;
	setupUi(this);

	// Load current shortcut configuration
	qcbForceCenter->setChecked(st.bForceCenter);
	qgbModifiers->setVisible(g.s.bExpert);

	if (st.bUsers) {
		qrbUsers->setChecked(true);
		qswStack->setCurrentWidget(qwUserPage);
	} else {
		qrbChannel->setChecked(true);
		qswStack->setCurrentWidget(qwChannelPage);
	}

	qcbLinks->setChecked(st.bLinks);
	qcbChildren->setChecked(st.bChildren);

	// Insert all known friends into the possible targets list
	const QMap<QString, QString> &friends = Database::getFriends();
	if (! friends.isEmpty()) {
		QMap<QString, QString>::const_iterator i;
		for (i = friends.constBegin(); i != friends.constEnd(); ++i) {
			qcbUser->addItem(i.key(), i.value());
			qmHashNames.insert(i.value(), i.key());
		}
		qcbUser->insertSeparator(qcbUser->count());
	}

	// If we are connected to a server also add all connected players with certificates to the list
	if (g.uiSession) {
		QMap<QString, QString> others;
		QMap<QString, QString>::const_iterator i;

		QReadLocker lock(& ClientUser::c_qrwlUsers);
		foreach(ClientUser *p, ClientUser::c_qmUsers) {
			if ((p->uiSession != g.uiSession) && p->qsFriendName.isEmpty() && ! p->qsHash.isEmpty()) {
				others.insert(p->qsName, p->qsHash);
				qmHashNames.insert(p->qsHash, p->qsName);
			}
		}

		for (i = others.constBegin(); i != others.constEnd(); ++i) {
			qcbUser->addItem(i.key(), i.value());
		}
	}

	QMap<QString, QString> users;

	foreach(const QString &hash, st.qlUsers) {
		if (qmHashNames.contains(hash))
			users.insert(qmHashNames.value(hash), hash);
		else
			users.insert(QString::fromLatin1("#%1").arg(hash), hash);
	}

	{
		QMap<QString, QString>::const_iterator i;
		for (i=users.constBegin(); i != users.constEnd(); ++i) {
			QListWidgetItem *itm = new QListWidgetItem(i.key());
			itm->setData(Qt::UserRole, i.value());
			qlwUsers->addItem(itm);
		}
	}

	// Now generate the tree of possible channel targets, first add the default ones
	QMap<int, QTreeWidgetItem *> qmTree;

	QTreeWidgetItem *root = new QTreeWidgetItem(qtwChannels, QStringList(tr("Root")));
	root->setData(0, Qt::UserRole, SHORTCUT_TARGET_ROOT);
	root->setExpanded(true);
	qmTree.insert(-1, root);

	QTreeWidgetItem *pitem = new QTreeWidgetItem(root, QStringList(tr("Parent")));
	pitem->setData(0, Qt::UserRole, SHORTCUT_TARGET_PARENT);
	pitem->setExpanded(true);
	qmTree.insert(-2, pitem);

	QTreeWidgetItem *current = new QTreeWidgetItem(pitem, QStringList(tr("Current")));
	current->setData(0, Qt::UserRole, SHORTCUT_TARGET_CURRENT);
	qmTree.insert(-3, current);

	for (int i = 0; i < 8; ++i) {
		QTreeWidgetItem *sub = new QTreeWidgetItem(current, QStringList(tr("Subchannel #%1").arg(i+1)));
		sub->setData(0, Qt::UserRole, SHORTCUT_TARGET_SUBCHANNEL - i);
		qmTree.insert(SHORTCUT_TARGET_SUBCHANNEL - i, sub);
	}

	for (int i = 0; i < 8; ++i) {
		QTreeWidgetItem *psub = new QTreeWidgetItem(pitem, QStringList(UPARROW + tr("Subchannel #%1").arg(i+1)));
		psub->setData(0, Qt::UserRole, SHORTCUT_TARGET_PARENT_SUBCHANNEL - i);
		qmTree.insert(SHORTCUT_TARGET_PARENT_SUBCHANNEL - i, psub);
	}

	// And if we are connected add the channels on the current server
	if (g.uiSession) {
		Channel *c = Channel::get(0);
		QTreeWidgetItem *sroot = new QTreeWidgetItem(qtwChannels, QStringList(c->qsName));
		qmTree.insert(0, sroot);
		iterateChannelChildren(sroot, c, qmTree);
	}

	qtwChannels->sortByColumn(0, Qt::AscendingOrder);

	QTreeWidgetItem *qtwi;
	if (g.uiSession) {
		qtwi = qmTree.value(ClientUser::get(g.uiSession)->cChannel->iId);
		if (qtwi)
			qtwChannels->scrollToItem(qtwi);
	}

	qtwi = qmTree.value(st.iChannel);
	if (qtwi) {
		qtwChannels->scrollToItem(qtwi);
		qtwChannels->setCurrentItem(qtwi);
	}

	qleGroup->setText(stTarget.qsGroup);
}

ShortcutTarget ShortcutTargetDialog::target() const {
	return stTarget;
}

void ShortcutTargetDialog::accept() {
	stTarget.bLinks = qcbLinks->isChecked();
	stTarget.bChildren = qcbChildren->isChecked();

	stTarget.bForceCenter = qcbForceCenter->isChecked();

	stTarget.qlUsers.clear();
	QList<QListWidgetItem *> ql = qlwUsers->findItems(QString(), Qt::MatchStartsWith);
	foreach(QListWidgetItem *itm, ql) {
		stTarget.qlUsers << itm->data(Qt::UserRole).toString();
	}

	QTreeWidgetItem *qtwi = qtwChannels->currentItem();
	if (qtwi) {
		stTarget.iChannel = qtwi->data(0, Qt::UserRole).toInt();
		stTarget.qsGroup = qleGroup->text().trimmed();
	}

	QDialog::accept();
}

void ShortcutTargetDialog::on_qrbUsers_clicked() {
	stTarget.bUsers = true;
	qswStack->setCurrentWidget(qwUserPage);
}

void ShortcutTargetDialog::on_qrbChannel_clicked() {
	stTarget.bUsers = false;
	qswStack->setCurrentWidget(qwChannelPage);
}

void ShortcutTargetDialog::on_qpbAdd_clicked() {
	if (qcbUser->currentIndex() < 0)
		return;

	QListWidgetItem *itm = new QListWidgetItem(qcbUser->currentText());
	itm->setData(Qt::UserRole, qcbUser->itemData(qcbUser->currentIndex()));
	qlwUsers->addItem(itm);
}

void ShortcutTargetDialog::on_qpbRemove_clicked() {
	QListWidgetItem *itm = qlwUsers->currentItem();
	delete itm;
}

ShortcutTargetWidget::ShortcutTargetWidget(QWidget *p) : QFrame(p) {
	qleTarget = new QLineEdit();
	qleTarget->setReadOnly(true);

	qtbEdit = new QToolButton();
	qtbEdit->setText(tr("..."));
	qtbEdit->setFocusPolicy(Qt::ClickFocus);
	qtbEdit->setObjectName(QLatin1String("qtbEdit"));

	QHBoxLayout *l = new QHBoxLayout(this);
	l->setContentsMargins(0,0,0,0);
	l->addWidget(qleTarget, 1);
	l->addWidget(qtbEdit);

	QMetaObject::connectSlotsByName(this);
}

QString ShortcutTargetWidget::targetString(const ShortcutTarget &st) {
	if (st.bUsers) {
		if (! st.qlUsers.isEmpty()) {
			QMap<QString, QString> hashes;

			QReadLocker lock(& ClientUser::c_qrwlUsers);
			foreach(ClientUser *p, ClientUser::c_qmUsers) {
				if (! p->qsHash.isEmpty()) {
					hashes.insert(p->qsHash, p->qsName);
				}
			}

			QStringList users;
			foreach(const QString &hash, st.qlUsers) {
				QString name;
				if (hashes.contains(hash)) {
					name = hashes.value(hash);
				} else {
					name = Database::getFriend(hash);
					if (name.isEmpty())
						name = QString::fromLatin1("#%1").arg(hash);
				}
				users << name;
			}

			users.sort();
			return users.join(tr(", "));
		}
	} else {
		if (st.iChannel < 0) {
			switch (st.iChannel) {
				case SHORTCUT_TARGET_ROOT:
					return tr("Root");
				case SHORTCUT_TARGET_PARENT:
					return tr("Parent");
				case SHORTCUT_TARGET_CURRENT:
					return tr("Current");
				default:
					if(st.iChannel <= SHORTCUT_TARGET_PARENT_SUBCHANNEL)
						return (UPARROW + tr("Subchannel #%1").arg(SHORTCUT_TARGET_PARENT_SUBCHANNEL + 1 - st.iChannel));
					else
						return tr("Subchannel #%1").arg(SHORTCUT_TARGET_CURRENT - st.iChannel);
			}
		} else {
			Channel *c = Channel::get(st.iChannel);
			if (c)
				return c->qsName;
			else
				return tr("Invalid");
		}
	}
	return tr("Empty");
}

ShortcutTarget ShortcutTargetWidget::target() const {
	return stTarget;
}

void ShortcutTargetWidget::setTarget(const ShortcutTarget &st) {
	stTarget = st;
	qleTarget->setText(ShortcutTargetWidget::targetString(st));
}

void ShortcutTargetWidget::on_qtbEdit_clicked() {
	ShortcutTargetDialog *std = new ShortcutTargetDialog(stTarget, this);
	if (std->exec() == QDialog::Accepted) {
		stTarget = std->target();
		qleTarget->setText(ShortcutTargetWidget::targetString(stTarget));

		// Qt bug? Who knows, but since there won't be focusOut events for this widget anymore,
		// we need to force the commit.
		QWidget *p = parentWidget();
		while (p) {
			if (QAbstractItemView *qaiv = qobject_cast<QAbstractItemView *>(p)) {
				QStyledItemDelegate *qsid = qobject_cast<QStyledItemDelegate *>(qaiv->itemDelegate());
				if (qsid) {
					QMetaObject::invokeMethod(qsid, "_q_commitDataAndCloseEditor",
					                          Qt::QueuedConnection, Q_ARG(QWidget*, this));
				}
				break;
			}
			p = p->parentWidget();
		}
	}
	delete std;
}

ShortcutDelegate::ShortcutDelegate(QObject *p) : QStyledItemDelegate(p) {
	QItemEditorFactory *factory = new QItemEditorFactory;

	factory->registerEditor(QVariant::List, new QStandardItemEditorCreator<ShortcutKeyWidget>());
	factory->registerEditor(QVariant::UInt, new QStandardItemEditorCreator<ShortcutActionWidget>());
	factory->registerEditor(QVariant::Int, new QStandardItemEditorCreator<ShortcutToggleWidget>());
	factory->registerEditor(static_cast<QVariant::Type>(QVariant::fromValue(ShortcutTarget()).userType()), new QStandardItemEditorCreator<ShortcutTargetWidget>());
	factory->registerEditor(QVariant::String, new QStandardItemEditorCreator<QWidget>());
	factory->registerEditor(QVariant::Invalid, new QStandardItemEditorCreator<QWidget>());
	setItemEditorFactory(factory);
}

ShortcutDelegate::~ShortcutDelegate() {
	delete itemEditorFactory();
	setItemEditorFactory(NULL);
}

QString ShortcutDelegate::displayText(const QVariant &item, const QLocale &loc) const {
	if (item.type() == QVariant::List) {
		return GlobalShortcutEngine::buttonText(item.toList());
	} else if (item.type() == QVariant::Int) {
		int v = item.toInt();
		if (v > 0)
			return tr("On");
		else if (v < 0)
			return tr("Off");
		else
			return tr("Toggle");
	} else if (item.type() == QVariant::UInt) {
		GlobalShortcut *gs = GlobalShortcutEngine::engine->qmShortcuts.value(item.toInt());
		if (gs)
			return gs->name;
		else
			return tr("Unassigned");
	} else if (item.userType() == QVariant::fromValue(ShortcutTarget()).userType()) {
		return ShortcutTargetWidget::targetString(item.value<ShortcutTarget>());
	}

	qWarning("ShortcutDelegate::displayText Unknown type %d", item.type());

	return QStyledItemDelegate::displayText(item,loc);
}

GlobalShortcutConfig::GlobalShortcutConfig(Settings &st) : ConfigWidget(st) {
	setupUi(this);
	installEventFilter(this);

	bool canSuppress = GlobalShortcutEngine::engine->canSuppress();
	bool canDisable = GlobalShortcutEngine::engine->canDisable();

	qwWarningContainer->setVisible(false);

	qtwShortcuts->setColumnCount(canSuppress ? 4 : 3);
	qtwShortcuts->setItemDelegate(new ShortcutDelegate(qtwShortcuts));

	qtwShortcuts->header()->setResizeMode(0, QHeaderView::Fixed);
	qtwShortcuts->header()->resizeSection(0, 150);
	qtwShortcuts->header()->setResizeMode(2, QHeaderView::Stretch);
	if (canSuppress)
		qtwShortcuts->header()->setResizeMode(3, QHeaderView::ResizeToContents);
	qcbEnableGlobalShortcuts->setVisible(canDisable);

#ifdef Q_OS_MAC
	// Help Mac users enable accessibility access for Mumble...
	if (QSysInfo::MacintoshVersion >= QSysInfo::MV_MAVERICKS) {
		qpbOpenAccessibilityPrefs->setHidden(true);
		label->setText(tr(
			"<html><head/><body>"
			"<p>"
			"Mumble can currently only use mouse buttons and keyboard modifier keys (Alt, Ctrl, Cmd, etc.) for global shortcuts."
			"</p>"
			"<p>"
			"If you want more flexibility, you can add Mumble as a trusted accessibility program in the Security & Privacy section "
			"of your Mac's System Preferences."
			"</p>"
			"<p>"
			"In the Security & Privacy preference pane, change to the Privacy tab. Then choose Accessibility (near the bottom) in "
			"the list to the left. Finally, add Mumble to the list of trusted accessibility programs."
			"</body></html>"
		));
	}
#endif
}

bool GlobalShortcutConfig::eventFilter(QObject *object, QEvent *event) {
#ifdef Q_OS_MAC
	if (event->type() == QEvent::WindowActivate) {
		if (! g.s.bSuppressMacEventTapWarning) {
			qwWarningContainer->setVisible(showWarning());
		}
	}
#endif
	return false;
}

bool GlobalShortcutConfig::showWarning() const {
#ifdef Q_OS_MAC
	if (QSysInfo::MacintoshVersion >= QSysInfo::MV_MAVERICKS) {
		return !AXIsProcessTrustedWithOptions(NULL);
	} else {
		return !QFile::exists(QLatin1String("/private/var/db/.AccessibilityAPIEnabled"));
	}
#endif
	return false;
}

void GlobalShortcutConfig::on_qpbOpenAccessibilityPrefs_clicked() {
	QStringList args;
	args << QLatin1String("/Applications/System Preferences.app");
	args << QLatin1String("/System/Library/PreferencePanes/UniversalAccessPref.prefPane");
	(void) QProcess::startDetached(QLatin1String("/usr/bin/open"), args);
}

void GlobalShortcutConfig::on_qpbSkipWarning_clicked() {
	// Store to both global and local settings.  The 'Skip' is live, as in
	// we don't expect the user to click Apply for their choice to work.
	g.s.bSuppressMacEventTapWarning = s.bSuppressMacEventTapWarning = true;
	qwWarningContainer->setVisible(false);
}

void GlobalShortcutConfig::commit() {
	qtwShortcuts->closePersistentEditor(qtwShortcuts->currentItem(), qtwShortcuts->currentColumn());
}

void GlobalShortcutConfig::on_qcbEnableGlobalShortcuts_stateChanged(int state) {
	bool b = state == Qt::Checked;
	qpbAdd->setEnabled(b);
	if (!b)
		qpbRemove->setEnabled(false);
	else
		qpbRemove->setEnabled(qtwShortcuts->currentItem() ? true : false);
	qtwShortcuts->setEnabled(b);

	// We have to enable this here. Otherwise, adding new shortcuts wouldn't work.
	GlobalShortcutEngine::engine->setEnabled(b);
}

void GlobalShortcutConfig::on_qpbAdd_clicked(bool) {
	commit();
	Shortcut sc;
	sc.iIndex = -1;
	sc.bSuppress = false;
	qlShortcuts << sc;
	reload();
}

void GlobalShortcutConfig::on_qpbRemove_clicked(bool) {
	commit();
	QTreeWidgetItem *qtwi = qtwShortcuts->currentItem();
	if (! qtwi)
		return;
	int idx = qtwShortcuts->indexOfTopLevelItem(qtwi);
	delete qtwi;
	qlShortcuts.removeAt(idx);
}

void GlobalShortcutConfig::on_qtwShortcuts_currentItemChanged(QTreeWidgetItem *item, QTreeWidgetItem *) {
	qpbRemove->setEnabled(item ? true : false);
}

void GlobalShortcutConfig::on_qtwShortcuts_itemChanged(QTreeWidgetItem *item, int) {
	int idx = qtwShortcuts->indexOfTopLevelItem(item);

	Shortcut &sc = qlShortcuts[idx];
	sc.iIndex = item->data(0, Qt::DisplayRole).toInt();
	sc.qvData = item->data(1, Qt::DisplayRole);
	sc.qlButtons = item->data(2, Qt::DisplayRole).toList();
	sc.bSuppress = item->checkState(3) == Qt::Checked;

	const ::GlobalShortcut *gs = GlobalShortcutEngine::engine->qmShortcuts.value(sc.iIndex);
	if (gs && sc.qvData.type() != gs->qvDefault.type()) {
		item->setData(1, Qt::DisplayRole, gs->qvDefault);
	}
}

QString GlobalShortcutConfig::title() const {
	return tr("Shortcuts");
}

QIcon GlobalShortcutConfig::icon() const {
	return QIcon(QLatin1String("skin:config_shortcuts.png"));
}

void GlobalShortcutConfig::load(const Settings &r) {
	qlShortcuts = r.qlShortcuts;
	bExpert = r.bExpert;

	// The 'Skip' button is supposed to be live, meaning users do not need to click Apply for
	// their choice of skipping to apply.
	//
	// To make this work well, we set the setting on load. This is to make 'Reset' and 'Restore Defaults'
	// work as expected.
	g.s.bSuppressMacEventTapWarning = s.bSuppressMacEventTapWarning = r.bSuppressMacEventTapWarning;
	if (! g.s.bSuppressMacEventTapWarning) {
		qwWarningContainer->setVisible(showWarning());
	}

	qcbEnableGlobalShortcuts->setCheckState(r.bShortcutEnable ? Qt::Checked : Qt::Unchecked);
	on_qcbEnableGlobalShortcuts_stateChanged(qcbEnableGlobalShortcuts->checkState());
	reload();
}

void GlobalShortcutConfig::save() const {
	s.qlShortcuts = qlShortcuts;
	s.bShortcutEnable = qcbEnableGlobalShortcuts->checkState() == Qt::Checked;
}

QTreeWidgetItem *GlobalShortcutConfig::itemForShortcut(const Shortcut &sc) const {
	QTreeWidgetItem *item = new QTreeWidgetItem();
	::GlobalShortcut *gs = GlobalShortcutEngine::engine->qmShortcuts.value(sc.iIndex);

	item->setData(0, Qt::DisplayRole, static_cast<unsigned int>(sc.iIndex));
	if (sc.qvData.isValid() && gs && (sc.qvData.type() == gs->qvDefault.type()))
		item->setData(1, Qt::DisplayRole, sc.qvData);
	else if (gs)
		item->setData(1, Qt::DisplayRole, gs->qvDefault);
	item->setData(2, Qt::DisplayRole, sc.qlButtons);
	item->setCheckState(3, sc.bSuppress ? Qt::Checked : Qt::Unchecked);
	item->setFlags(item->flags() | Qt::ItemIsEditable);


	if (gs) {
		if (! gs->qsToolTip.isEmpty())
			item->setData(0, Qt::ToolTipRole, gs->qsToolTip);
		if (! gs->qsWhatsThis.isEmpty())
			item->setData(0, Qt::WhatsThisRole, gs->qsWhatsThis);
	}

	item->setData(2, Qt::ToolTipRole, tr("Shortcut button combination."));
	item->setData(2, Qt::WhatsThisRole, tr("<b>This is the global shortcut key combination.</b><br />"
	                                       "Click this field and then press the desired key/button combo "
	                                       "to rebind. Double-click to clear."));

	item->setData(3, Qt::ToolTipRole, tr("Suppress keys from other applications"));
	item->setData(3, Qt::WhatsThisRole, tr("<b>This hides the button presses from other applications.</b><br />"
	                                       "Enabling this will hide the button (or the last button of a multi-button combo) "
	                                       "from other applications. Note that not all buttons can be suppressed."));

	return item;
}

void GlobalShortcutConfig::reload() {
	qStableSort(qlShortcuts);
	qtwShortcuts->clear();
	foreach(const Shortcut &sc, qlShortcuts) {
		QTreeWidgetItem *item = itemForShortcut(sc);
		::GlobalShortcut *gs = GlobalShortcutEngine::engine->qmShortcuts.value(sc.iIndex);
		qtwShortcuts->addTopLevelItem(item);
		if (gs && gs->bExpert && ! bExpert)
			item->setHidden(true);
	}
#ifdef Q_OS_MAC
	if (! g.s.bSuppressMacEventTapWarning) {
		qwWarningContainer->setVisible(showWarning());
	} else {
		qwWarningContainer->setVisible(false);
	}
#endif
}

void GlobalShortcutConfig::accept() const {
	GlobalShortcutEngine::engine->bNeedRemap = true;
	GlobalShortcutEngine::engine->needRemap();
	GlobalShortcutEngine::engine->setEnabled(g.s.bShortcutEnable);
}

bool GlobalShortcutConfig::expert(bool exp) {
	bExpert = exp;
	reload();
	return true;
}


GlobalShortcutEngine::GlobalShortcutEngine(QObject *p) : QThread(p) {
	bNeedRemap = true;
	needRemap();
}

GlobalShortcutEngine::~GlobalShortcutEngine() {
	QSet<ShortcutKey *> qs;
	foreach(const QList<ShortcutKey*> &ql, qlShortcutList)
		qs += ql.toSet();

	foreach(ShortcutKey *sk, qs)
		delete sk;
}

void GlobalShortcutEngine::remap() {
	bNeedRemap = false;

	QSet<ShortcutKey *> qs;
	foreach(const QList<ShortcutKey*> &ql, qlShortcutList)
		qs += ql.toSet();

	foreach(ShortcutKey *sk, qs)
		delete sk;

	qlButtonList.clear();
	qlShortcutList.clear();
	qlDownButtons.clear();

	foreach(const Shortcut &sc, g.s.qlShortcuts) {
		GlobalShortcut *gs = qmShortcuts.value(sc.iIndex);
		if (gs && ! sc.qlButtons.isEmpty()) {
			ShortcutKey *sk = new ShortcutKey;
			sk->s = sc;
			sk->iNumUp = sc.qlButtons.count();
			sk->gs = gs;

			foreach(const QVariant &button, sc.qlButtons) {
				int idx = qlButtonList.indexOf(button);
				if (idx == -1) {
					qlButtonList << button;
					qlShortcutList << QList<ShortcutKey *>();
					idx = qlButtonList.count() - 1;
				}
				qlShortcutList[idx] << sk;
			}
		}
	}
}

void GlobalShortcutEngine::run() {
}

bool GlobalShortcutEngine::canSuppress() {
	return false;
}

void GlobalShortcutEngine::setEnabled(bool) {
}

bool GlobalShortcutEngine::enabled() {
	return true;
}

bool GlobalShortcutEngine::canDisable() {
	return false;
}

void GlobalShortcutEngine::resetMap() {
	tReset.restart();
	qlActiveButtons.clear();
}

void GlobalShortcutEngine::needRemap() {
}

bool GlobalShortcutEngine::handleButton(const QVariant &button, bool down) {
	bool already = qlDownButtons.contains(button);
	if (already == down)
		return qlSuppressed.contains(button);
	if (down)
		qlDownButtons << button;
	else
		qlDownButtons.removeAll(button);

	if (tReset.elapsed() > 100000) {
		if (down) {
			qlActiveButtons.removeAll(button);
			qlActiveButtons << button;
		}
		emit buttonPressed(! down);
	}

	if (down) {
		AudioInputPtr ai = g.ai;
		if (ai.get())
			ai->tIdle.restart();
	}

	int idx = qlButtonList.indexOf(button);
	if (idx == -1)
		return false;

	bool suppress = false;

	foreach(ShortcutKey *sk, qlShortcutList.at(idx)) {
		if (down) {
			sk->iNumUp--;
			if (sk->iNumUp == 0) {
				GlobalShortcut *gs = sk->gs;
				if (sk->s.bSuppress) {
					suppress = true;
					qlSuppressed << button;
				}
				if (! gs->qlActive.contains(sk->s.qvData)) {
					gs->qlActive << sk->s.qvData;
					emit gs->triggered(true, sk->s.qvData);
					emit gs->down(sk->s.qvData);
				}
			} else if (sk->iNumUp < 0) {
				sk->iNumUp = 0;
			}
		} else {
			if (qlSuppressed.contains(button)) {
				suppress = true;
				qlSuppressed.removeAll(button);
			}
			sk->iNumUp++;
			if (sk->iNumUp == 1) {
				GlobalShortcut *gs = sk->gs;
				if (gs->qlActive.contains(sk->s.qvData)) {
					gs->qlActive.removeAll(sk->s.qvData);
					emit gs->triggered(false, sk->s.qvData);
				}
			} else if (sk->iNumUp > sk->s.qlButtons.count()) {
				sk->iNumUp = sk->s.qlButtons.count();
			}
		}
	}
	return suppress;
}

void GlobalShortcutEngine::add(GlobalShortcut *gs) {
	if (! GlobalShortcutEngine::engine) {
		GlobalShortcutEngine::engine = GlobalShortcutEngine::platformInit();
		GlobalShortcutEngine::engine->setEnabled(g.s.bShortcutEnable);
	}

	GlobalShortcutEngine::engine->qmShortcuts.insert(gs->idx, gs);
	GlobalShortcutEngine::engine->bNeedRemap = true;
	GlobalShortcutEngine::engine->needRemap();
}

void GlobalShortcutEngine::remove(GlobalShortcut *gs) {
	engine->qmShortcuts.remove(gs->idx);
	engine->bNeedRemap = true;
	engine->needRemap();
	if (engine->qmShortcuts.isEmpty()) {
		delete engine;
		GlobalShortcutEngine::engine = NULL;
	}
}

QString GlobalShortcutEngine::buttonText(const QList<QVariant> &list) {
	QStringList keys;

	foreach(QVariant button, list) {
		QString id = GlobalShortcutEngine::engine->buttonName(button);
		if (! id.isEmpty())
			keys << id;
	}
	return keys.join(QLatin1String(" + "));
}

void GlobalShortcutEngine::prepareInput() {
}

GlobalShortcut::GlobalShortcut(QObject *p, int index, QString qsName, bool expert, QVariant def) : QObject(p) {
	idx = index;
	name=qsName;
	bExpert = expert;
	qvDefault = def;
	GlobalShortcutEngine::add(this);
}

GlobalShortcut::~GlobalShortcut() {
	GlobalShortcutEngine::remove(this);
}
