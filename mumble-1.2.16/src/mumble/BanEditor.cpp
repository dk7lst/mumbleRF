/* copyright (C) 2005-2011, Thorvald Natvig <thorvald@natvig.com>

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

#include "BanEditor.h"

#include "Channel.h"
#include "Global.h"
#include "Net.h"
#include "ServerHandler.h"

BanEditor::BanEditor(const MumbleProto::BanList &msg, QWidget *p) : QDialog(p) {
	setupUi(this);

	qlBans.clear();
	for (int i=0;i < msg.bans_size(); ++i) {
		const MumbleProto::BanList_BanEntry &be = msg.bans(i);
		Ban b;
		b.haAddress = be.address();
		b.iMask = be.mask();
		b.qsUsername = u8(be.name());
		b.qsHash = u8(be.hash());
		b.qsReason = u8(be.reason());
		b.qdtStart = QDateTime::fromString(u8(be.start()), Qt::ISODate);
		b.qdtStart.setTimeSpec(Qt::UTC);
		if (! b.qdtStart.isValid())
			b.qdtStart = QDateTime::currentDateTime();
		b.iDuration = be.duration();
		if (b.isValid())
			qlBans << b;
	}

	refreshBanList();
}

void BanEditor::accept() {
	MumbleProto::BanList msg;

	foreach(const Ban &b, qlBans) {
		MumbleProto::BanList_BanEntry *be = msg.add_bans();
		be->set_address(b.haAddress.toStdString());
		be->set_mask(b.iMask);
		be->set_name(u8(b.qsUsername));
		be->set_hash(u8(b.qsHash));
		be->set_reason(u8(b.qsReason));
		be->set_start(u8(b.qdtStart.toString(Qt::ISODate)));
		be->set_duration(b.iDuration);
	}

	g.sh->sendMessage(msg);
	QDialog::accept();
}

void BanEditor::on_qlwBans_currentRowChanged() {
	int idx = qlwBans->currentRow();
	if (idx < 0)
		return;
	const Ban &ban = qlBans.at(idx);

	int maskbits = ban.iMask;

	const QHostAddress &addr = ban.haAddress.toAddress();
	qleIP->setText(addr.toString());
	if (! ban.haAddress.isV6())
		maskbits -= 96;
	qsbMask->setValue(maskbits);
	qlUser->setText(ban.qsUsername);
	qlHash->setText(ban.qsHash);
	qleReason->setText(ban.qsReason);
	qdteStart->setDateTime(ban.qdtStart.toLocalTime());
	qdteEnd->setDateTime(ban.qdtStart.toLocalTime().addSecs(ban.iDuration));

}

Ban BanEditor::toBan(bool &ok) {
	Ban b;

	QHostAddress addr;

	ok = addr.setAddress(qleIP->text());

	if (ok) {
		b.haAddress = addr;
		b.iMask = qsbMask->value();
		if (! b.haAddress.isV6())
			b.iMask += 96;
		b.qsUsername = qlUser->text();
		b.qsHash = qlHash->text();
		b.qsReason = qleReason->text();
		b.qdtStart = qdteStart->dateTime().toUTC();
		const QDateTime &qdte = qdteEnd->dateTime();
		if (qdte <= b.qdtStart)
			b.iDuration = 0;
		else
			b.iDuration = b.qdtStart.secsTo(qdte);

		ok = b.isValid();
	}
	return b;
}

void BanEditor::on_qpbAdd_clicked() {
	QHostAddress addr;

	bool ok;

	qdteStart->setDateTime(QDateTime::currentDateTime());

	Ban b = toBan(ok);

	if (ok) {
		b.qsHash = QString();
		b.qsUsername = QString();
		qlBans << b;
		refreshBanList();
		qlwBans->setCurrentRow(qlBans.indexOf(b));
	}
}

void BanEditor::on_qpbUpdate_clicked() {
	int idx = qlwBans->currentRow();
	if (idx >= 0) {
		bool ok;
		Ban b = toBan(ok);
		if (ok) {
			qlBans.replace(idx, b);
			refreshBanList();
			qlwBans->setCurrentRow(qlBans.indexOf(b));
		}
	}
}

void BanEditor::on_qpbRemove_clicked() {
	int idx = qlwBans->currentRow();
	if (idx >= 0)
		qlBans.removeAt(idx);
	refreshBanList();
}

void BanEditor::refreshBanList() {
	qlwBans->clear();

	qSort(qlBans);

	foreach(const Ban &ban, qlBans) {
		int maskbits = ban.iMask;
		const QHostAddress &addr=ban.haAddress.toAddress();
		if (! ban.haAddress.isV6())
			maskbits -= 96;
		QString qs = QString::fromLatin1("%1/%2").arg(addr.toString()).arg(maskbits);
		qlwBans->addItem(qs);
	}
}
