// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "UserEdit.h"

#include <QItemSelectionModel>

#include "Channel.h"
#include "ServerHandler.h"
#include "User.h"
#include "UserListModel.h"

// We define a global macro called 'g'. This can lead to issues when included code uses 'g' as a type or parameter name (like protobuf 3.7 does). As such, for now, we have to make this our last include.
#include "Global.h"

UserEdit::UserEdit(const MumbleProto::UserList &userList, QWidget *p)
	: QDialog(p)
	, m_model(new UserListModel(userList, this))
	, m_filter(new UserListFilterProxyModel(this)) {

	setupUi(this);

	const int userCount = userList.users_size();
	setWindowTitle(tr("Registered users: %n account(s)", "", userCount));

	m_filter->setSourceModel(m_model);
	qtvUserList->setModel(m_filter);

	QItemSelectionModel *selectionModel = qtvUserList->selectionModel();
	connect(selectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
	        this, SLOT(onSelectionChanged(QItemSelection,QItemSelection)));
	connect(selectionModel, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
	        this, SLOT(onCurrentRowChanged(QModelIndex,QModelIndex)));

	qtvUserList->setFocus();
	qtvUserList->setContextMenuPolicy(Qt::CustomContextMenu);

#if QT_VERSION >= 0x050000
	qtvUserList->header()->setSectionResizeMode(UserListModel::COL_NICK, QHeaderView::Stretch);
	if (!m_model->isLegacy()) {
		qtvUserList->header()->setSectionResizeMode(UserListModel::COL_INACTIVEDAYS, QHeaderView::ResizeToContents);
		qtvUserList->header()->setSectionResizeMode(UserListModel::COL_LASTCHANNEL, QHeaderView::Stretch);
	}
#else
	qtvUserList->header()->setResizeMode(UserListModel::COL_NICK, QHeaderView::Stretch);
	if (!m_model->isLegacy()) {
		qtvUserList->header()->setResizeMode(UserListModel::COL_INACTIVEDAYS, QHeaderView::ResizeToContents);
		qtvUserList->header()->setResizeMode(UserListModel::COL_LASTCHANNEL, QHeaderView::Stretch);
	}
#endif

	if (m_model->isLegacy()) {
		qlInactive->hide();
		qsbInactive->hide();
		qcbInactive->hide();
	}

	qtvUserList->sortByColumn(UserListModel::COL_NICK, Qt::AscendingOrder);
}

void UserEdit::accept() {
	if (m_model->isUserListDirty()) {
		MumbleProto::UserList userList = m_model->getUserListUpdate();
		g.sh->sendMessage(userList);
	}

	QDialog::accept();
}

void UserEdit::on_qlSearch_textChanged(QString pattern) {
	m_filter->setFilterWildcard(pattern);
}


void UserEdit::on_qpbRename_clicked() {
	QModelIndex current = qtvUserList->selectionModel()->currentIndex();
	if (!current.isValid())
		return;

	QModelIndex nickIndex = current.sibling(current.row(), UserListModel::COL_NICK);
	qtvUserList->edit(nickIndex);
}

void UserEdit::on_qpbRemove_clicked() {
	m_filter->removeRowsInSelection(qtvUserList->selectionModel()->selection());
}

void UserEdit::on_qtvUserList_customContextMenuRequested(const QPoint &point) {
	QMenu *menu = new QMenu(this);

	if (qtvUserList->selectionModel()->currentIndex().isValid()) {
		QAction *renameAction = menu->addAction(tr("Rename"));
		connect(renameAction, SIGNAL(triggered()),
		        this, SLOT(on_qpbRename_clicked()));

		menu->addSeparator();
	}

	QAction *removeMenuAction = menu->addAction(tr("Remove"));
	connect(removeMenuAction, SIGNAL(triggered()),
	        this, SLOT(on_qpbRemove_clicked()));

	menu->exec(qtvUserList->mapToGlobal(point));
	delete menu;
}

void UserEdit::onSelectionChanged(const QItemSelection& /*selected*/, const QItemSelection& /*deselected*/) {
	const bool somethingSelected = !(qtvUserList->selectionModel()->selection().empty());
	qpbRemove->setEnabled(somethingSelected);
}

void UserEdit::onCurrentRowChanged(const QModelIndex &current, const QModelIndex &) {
	qpbRename->setEnabled(current.isValid());
}

void UserEdit::on_qsbInactive_valueChanged(int) {
	updateInactiveDaysFilter();
}

void UserEdit::on_qcbInactive_currentIndexChanged(int) {
	updateInactiveDaysFilter();
}

void UserEdit::updateInactiveDaysFilter() {
	const int timespanUnit = qcbInactive->currentIndex();
	const int timespanCount = qsbInactive->value();

	int minimumInactiveDays = 0;
	switch (timespanUnit) {
		case TU_DAYS:
			minimumInactiveDays = timespanCount;
			break;
		case TU_WEEKS:
			minimumInactiveDays = timespanCount * 7;
			break;
		case TU_MONTHS:
			minimumInactiveDays = timespanCount * 30;
			break;
		case TU_YEARS:
			minimumInactiveDays = timespanCount * 365;
			break;
		default:
			break;
	}

	m_filter->setFilterMinimumInactiveDays(minimumInactiveDays);
}


UserListFilterProxyModel::UserListFilterProxyModel(QObject *parent_)
	: QSortFilterProxyModel(parent_)
	, m_minimumInactiveDays(0) {

	setFilterKeyColumn(UserListModel::COL_NICK);
	setFilterCaseSensitivity(Qt::CaseInsensitive);
	setSortLocaleAware(true);
	setDynamicSortFilter(true);
}

bool UserListFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
	if(!QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent)) {
		return false;
	}

	const QModelIndex inactiveDaysIdx = sourceModel()->index(source_row,
	                                                         UserListModel::COL_INACTIVEDAYS,
	                                                         source_parent);

	bool ok;
	const int inactiveDays = inactiveDaysIdx.data().toInt(&ok);

	// If inactiveDaysIdx doesn't store an int the account hasn't been seen yet and mustn't be filtered
	if (ok && inactiveDays < m_minimumInactiveDays)
		return false;

	return true;
}

void UserListFilterProxyModel::setFilterMinimumInactiveDays(int minimumInactiveDays) {
	m_minimumInactiveDays = minimumInactiveDays;
	invalidateFilter();
}

void UserListFilterProxyModel::removeRowsInSelection(const QItemSelection &selection) {
	QItemSelection sourceSelection = mapSelectionToSource(selection);
	qobject_cast<UserListModel*>(sourceModel())->removeRowsInSelection(sourceSelection);
}

