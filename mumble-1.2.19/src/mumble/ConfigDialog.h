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

#ifndef CONFIGDIALOG_H_
#define CONFIGDIALOG_H_

#include "ConfigWidget.h"
#include "Settings.h"

#include "ui_ConfigDialog.h"

class ConfigDialog : public QDialog, public Ui::ConfigDialog {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(ConfigDialog)
	protected:
		QHash<ConfigWidget *, QWidget *> qhPages;
		QMap<unsigned int, ConfigWidget *> qmWidgets;
		QMap<QListWidgetItem *, ConfigWidget *> qmIconWidgets;
		void addPage(ConfigWidget *aw, unsigned int idx);
		Settings s;

	public:
		ConfigDialog(QWidget *p = NULL);
		~ConfigDialog();
#ifdef Q_OS_MAC
	protected:
		void setupMacToolbar(bool expert);
		void removeMacToolbar();
	public:
		void updateExpert(bool expert);
		void on_widgetSelected(ConfigWidget *);
#endif
	public slots:
		void on_pageButtonBox_clicked(QAbstractButton *);
		void on_dialogButtonBox_clicked(QAbstractButton *);
		void on_qlwIcons_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
		void on_qcbExpert_clicked(bool);
		void apply();
		void accept();
};

#endif
