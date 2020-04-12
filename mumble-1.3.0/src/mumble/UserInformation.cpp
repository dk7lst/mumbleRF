// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "UserInformation.h"

#include "Audio.h"
#include "CELTCodec.h"
#include "HostAddress.h"
#include "ServerHandler.h"
#include "ViewCert.h"

// We define a global macro called 'g'. This can lead to issues when included code uses 'g' as a type or parameter name (like protobuf 3.7 does). As such, for now, we have to make this our last include.
#include "Global.h"

static QString decode_utf8_qssl_string(const QString &input) {
	QString i = input;
	return QUrl::fromPercentEncoding(i.replace(QLatin1String("\\x"), QLatin1String("%")).toLatin1());
}

#if QT_VERSION >= 0x050000
static QString decode_utf8_qssl_string(const QStringList &list) {
	if (list.count() > 0) {
		return decode_utf8_qssl_string(list.at(0));
	}
	return QString();
}
#endif

UserInformation::UserInformation(const MumbleProto::UserStats &msg, QWidget *p) : QDialog(p) {
	setupUi(this);

	uiSession = msg.session();

	qtTimer = new QTimer(this);
	connect(qtTimer, SIGNAL(timeout()), this, SLOT(tick()));
	qtTimer->start(6000);

	qgbConnection->setVisible(false);

	qlOpus->setText(tr("Not Reported"));

	update(msg);
	resize(sizeHint());

	qfCertificateFont = qlCertificate->font();
}

unsigned int UserInformation::session() const {
	return uiSession;
}

void UserInformation::tick() {
	if (bRequested)
		return;

	bRequested = true;

	g.sh->requestUserStats(uiSession, true);
}

void UserInformation::on_qpbCertificate_clicked() {
	ViewCert *vc = new ViewCert(qlCerts, this);
	vc->setWindowModality(Qt::WindowModal);
	vc->setAttribute(Qt::WA_DeleteOnClose, true);
	vc->show();
}

QString UserInformation::secsToString(unsigned int secs) {
	QStringList qsl;

	int weeks = secs / (60 * 60 * 24 * 7);
	secs = secs % (60 * 60 * 24 * 7);
	int days = secs / (60 * 60 * 24);
	secs = secs % (60 * 60 * 24);
	int hours = secs / (60 * 60);
	secs = secs % (60 * 60);
	int minutes = secs / 60;
	int seconds = secs % 60;

	if (weeks)
		qsl << tr("%1w").arg(weeks);
	if (days)
		qsl << tr("%1d").arg(days);
	if (hours)
		qsl << tr("%1h").arg(hours);
	if (minutes || hours)
		qsl << tr("%1m").arg(minutes);
	qsl << tr("%1s").arg(seconds);

	return qsl.join(QLatin1String(" "));
}

void UserInformation::update(const MumbleProto::UserStats &msg) {
	bRequested = false;

	bool showcon = false;

	ClientUser *cu = ClientUser::get(uiSession);
	if (cu)
		setWindowTitle(cu->qsName);

	if (msg.certificates_size() > 0) {
		showcon = true;
		qlCerts.clear();
		for (int i=0;i<msg.certificates_size(); ++i) {
			const std::string &s = msg.certificates(i);
			QList<QSslCertificate> certs = QSslCertificate::fromData(QByteArray(s.data(), static_cast<int>(s.length())), QSsl::Der);
			qlCerts <<certs;
		}
		if (! qlCerts.isEmpty()) {
			qpbCertificate->setEnabled(true);

			const QSslCertificate &cert = qlCerts.last();

#if QT_VERSION >= 0x050000
			const QMultiMap<QSsl::AlternativeNameEntryType, QString> &alts = cert.subjectAlternativeNames();
#else
			const QMultiMap<QSsl::AlternateNameEntryType, QString> &alts = cert.alternateSubjectNames();
#endif
			if (alts.contains(QSsl::EmailEntry))
				qlCertificate->setText(QStringList(alts.values(QSsl::EmailEntry)).join(tr(", ")));
			else
				qlCertificate->setText(decode_utf8_qssl_string(cert.subjectInfo(QSslCertificate::CommonName)));

			if (msg.strong_certificate()) {
				QFont f = qfCertificateFont;
				f.setBold(true);
				qlCertificate->setFont(f);
			} else {
				qlCertificate->setFont(qfCertificateFont);
			}
		} else {
			qpbCertificate->setEnabled(false);
			qlCertificate->setText(QString());
		}
	}
	if (msg.has_address()) {
		showcon = true;
		HostAddress ha(msg.address());
		qlAddress->setText(ha.toString());
	}
	if (msg.has_version()) {
		showcon = true;

		const MumbleProto::Version &mpv = msg.version();

		qlVersion->setText(tr("%1 (%2)").arg(MumbleVersion::toString(mpv.version())).arg(u8(mpv.release())));
		qlOS->setText(tr("%1 (%2)").arg(u8(mpv.os())).arg(u8(mpv.os_version())));
	}
	if (msg.celt_versions_size() > 0) {
		QStringList qsl;
		for (int i=0;i<msg.celt_versions_size(); ++i) {
			int v = msg.celt_versions(i);
			CELTCodec *cc = g.qmCodecs.value(v);
			if (cc)
				qsl << cc->version();
			else
				qsl << QString::number(v, 16);
		}
		qlCELT->setText(qsl.join(tr(", ")));
	}
	if (msg.has_opus()) {
		qlOpus->setText(msg.opus() ? tr("Supported") : tr("Not Supported"));
	}
	if (showcon)
		qgbConnection->setVisible(true);

	qlTCPCount->setText(QString::number(msg.tcp_packets()));
	qlUDPCount->setText(QString::number(msg.udp_packets()));

	qlTCPAvg->setText(QString::number(msg.tcp_ping_avg(), 'f', 2));
	qlUDPAvg->setText(QString::number(msg.udp_ping_avg(), 'f', 2));

	qlTCPVar->setText(QString::number(msg.tcp_ping_var() > 0.0f ? sqrtf(msg.tcp_ping_var()) : 0.0f, 'f', 2));
	qlUDPVar->setText(QString::number(msg.udp_ping_var() > 0.0f ? sqrtf(msg.udp_ping_var()) : 0.0f, 'f', 2));

	if (msg.has_from_client() && msg.has_from_server()) {
		qgbUDP->setVisible(true);
		const MumbleProto::UserStats_Stats &from = msg.from_client();
		qlFromGood->setText(QString::number(from.good()));
		qlFromLate->setText(QString::number(from.late()));
		qlFromLost->setText(QString::number(from.lost()));
		qlFromResync->setText(QString::number(from.resync()));

		const MumbleProto::UserStats_Stats &to = msg.from_server();
		qlToGood->setText(QString::number(to.good()));
		qlToLate->setText(QString::number(to.late()));
		qlToLost->setText(QString::number(to.lost()));
		qlToResync->setText(QString::number(to.resync()));

		quint32 allFromPackets = from.good() + from.late() + from.lost();
		qlFromLatePercent->setText(QString::number(allFromPackets > 0 ? from.late() * 100.0 / allFromPackets : 0., 'f', 2));
		qlFromLostPercent->setText(QString::number(allFromPackets > 0 ? from.lost() * 100.0 / allFromPackets : 0., 'f', 2));

		quint32 allToPackets = to.good() + to.late() + to.lost();
		qlToLatePercent->setText(QString::number(allToPackets > 0 ? to.late() * 100.0 / allToPackets : 0., 'f', 2));
		qlToLostPercent->setText(QString::number(allToPackets > 0 ? to.lost() * 100.0 / allToPackets : 0., 'f', 2));
	} else {
		qgbUDP->setVisible(false);
	}

	if (msg.has_onlinesecs()) {
		if (msg.has_idlesecs())
			qlTime->setText(tr("%1 online (%2 idle)").arg(secsToString(msg.onlinesecs()), secsToString(msg.idlesecs())));
		else
			qlTime->setText(tr("%1 online").arg(secsToString(msg.onlinesecs())));
	}
	if (msg.has_bandwidth()) {
		qlBandwidth->setVisible(true);
		qliBandwidth->setVisible(true);
		qlBandwidth->setText(tr("%1 kbit/s").arg(msg.bandwidth() / 125.0, 0, 'f', 1));
	} else {
		qlBandwidth->setVisible(false);
		qliBandwidth->setVisible(false);
		qlBandwidth->setText(QString());
	}
}
