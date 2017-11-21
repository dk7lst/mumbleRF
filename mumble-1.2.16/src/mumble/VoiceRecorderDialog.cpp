/* Copyright (C) 2010-2011, Stefan Hacker <dd0t@users.sourceforge.net>
   Copyright (C) 2010-2011, Benjamin Jemlich <pcgod@users.sourceforge.net>

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

#include "VoiceRecorderDialog.h"

#include "AudioOutput.h"
#include "Global.h"
#include "ServerHandler.h"
#include "VoiceRecorder.h"

VoiceRecorderDialog::VoiceRecorderDialog(QWidget *p) : QDialog(p), qtTimer(new QTimer(this)) {
	qtTimer->setObjectName(QLatin1String("qtTimer"));
	qtTimer->setInterval(200);
	setupUi(this);

	qleTargetDirectory->setText(g.s.qsRecordingPath);
	qleFilename->setText(g.s.qsRecordingFile);
	qrbDownmix->setChecked(g.s.rmRecordingMode == Settings::RecordingMixdown);
	qrbMultichannel->setChecked(g.s.rmRecordingMode == Settings::RecordingMultichannel);

	QString qsTooltip = QString::fromLatin1(
	                        "%1"
	                        "<table>"
	                        "  <tr>"
	                        "    <td width=\"25%\">%user</td>"
	                        "    <td>%2</td>"
	                        "  </tr>"
	                        "  <tr>"
	                        "    <td>%date</td>"
	                        "    <td>%3</td>"
	                        "  </tr>"
	                        "  <tr>"
	                        "    <td>%time</td>"
	                        "    <td>%4</td>"
	                        "  </tr>"
	                        "  <tr>"
	                        "    <td>%host</td>"
	                        "    <td>%5</td>"
	                        "  </tr>"
	                        "</table>").
	                    arg(tr("Valid variables are:")).
	                    arg(tr("Inserts the user's name")).
	                    arg(tr("Inserts the current date")).
	                    arg(tr("Inserts the current time")).
	                    arg(tr("Inserts the hostname"));

	qleTargetDirectory->setToolTip(qsTooltip);
	qleFilename->setToolTip(qsTooltip);

	// Populate available codecs
	Q_ASSERT(VoiceRecorderFormat::kEnd != 0);
	for (int fm = 0; fm < VoiceRecorderFormat::kEnd; fm++) {
		qcbFormat->addItem(VoiceRecorderFormat::getFormatDescription(static_cast<VoiceRecorderFormat::Format>(fm)));
	}

	if (g.s.iRecordingFormat < 0 || g.s.iRecordingFormat > VoiceRecorderFormat::kEnd)
		g.s.iRecordingFormat = 0;

	qcbFormat->setCurrentIndex(g.s.iRecordingFormat);
}

VoiceRecorderDialog::~VoiceRecorderDialog() {
	reset();
}

void VoiceRecorderDialog::closeEvent(QCloseEvent *evt) {
	if (g.sh) {
		VoiceRecorderPtr recorder(g.sh->recorder);
		if (recorder && recorder->isRunning()) {
			int ret = QMessageBox::warning(this,
			                               tr("Recorder"),
			                               tr("Closing the recorder will stop your current recording. Do you really want to close the recorder?"),
			                               QMessageBox::Yes | QMessageBox::No,
			                               QMessageBox::No);

			if (ret == QMessageBox::No) {
				evt->ignore();
				return;
			}

			recorder->stop();
		}
	}

	g.s.qsRecordingPath = qleTargetDirectory->text();
	g.s.qsRecordingFile = qleFilename->text();
	if (qrbDownmix->isChecked())
		g.s.rmRecordingMode = Settings::RecordingMixdown;
	else
		g.s.rmRecordingMode = Settings::RecordingMultichannel;

	int i = qcbFormat->currentIndex();
	g.s.iRecordingFormat = (i == -1) ? 0 : i;

	reset();
	evt->accept();

	QDialog::closeEvent(evt);
}

void VoiceRecorderDialog::on_qpbStart_clicked() {
	if (!g.uiSession || !g.sh) {
		QMessageBox::critical(this,
		                      tr("Recorder"),
		                      tr("Unable to start recording. Not connected to a server."));
		reset();
		return;
	}

	if (g.sh->uiVersion < 0x010203) {
		QMessageBox::critical(this,
		                      tr("Recorder"),
		                      tr("The server you are currently connected to is version 1.2.2 or older. "
		                         "For privacy reasons, recording on servers of versions older than 1.2.3 "
		                         "is not possible.\nPlease contact your server administrator for further "
		                         "information."));
		return;
	}

	if (g.sh->recorder) {
		QMessageBox::information(this,
		                         tr("Recorder"),
		                         tr("There is already a recorder active for this server."));
		return;
	}

	// Check validity of input
	int ifm = qcbFormat->currentIndex();
	if (ifm == -1) {
		QMessageBox::critical(this,
		                      tr("Recorder"),
		                      tr("Please select a recording format."));
		return;
	}

	QString dstr = qleTargetDirectory->text();
	if (dstr.isEmpty()) {
		on_qpbTargetDirectoryBrowse_clicked();
		dstr = qleTargetDirectory->text();
		if (dstr.isEmpty())
			return;
	}
	QDir dir(dstr);

	QFileInfo fi(qleFilename->text());
	QString basename(fi.baseName());
	QString suffix(fi.completeSuffix());
	if (suffix.isEmpty())
		suffix = VoiceRecorderFormat::getFormatDefaultExtension(static_cast<VoiceRecorderFormat::Format>(ifm));


	if (basename.isEmpty()) {
		basename = QLatin1String("%user");
	}

	qleFilename->setText(basename);

	AudioOutputPtr ao(g.ao);
	if (!ao)
		return;

	g.sh->announceRecordingState(true);

	// Create the recorder
	g.sh->recorder.reset(new VoiceRecorder(this));
	VoiceRecorderPtr recorder(g.sh->recorder);

	// Wire it up
	connect(&*recorder, SIGNAL(recording_started()), this, SLOT(onRecorderStarted()));
	connect(&*recorder, SIGNAL(recording_stopped()), this, SLOT(onRecorderStopped()));
	connect(&*recorder, SIGNAL(error(int, QString)), this, SLOT(onRecorderError(int, QString)));

	// Configure it
	recorder->setSampleRate(ao->getMixerFreq());
	recorder->setFileName(dir.absoluteFilePath(basename + QLatin1Char('.') + suffix));
	recorder->setMixDown(qrbDownmix->isChecked());
	recorder->setFormat(static_cast<VoiceRecorderFormat::Format>(ifm));

	recorder->start();

	qpbStart->setDisabled(true);
	qpbStop->setEnabled(true);
	qgbMode->setDisabled(true);
	qgbOutput->setDisabled(true);
}

void VoiceRecorderDialog::on_qpbStop_clicked() {
	if (!g.sh) {
		reset();
		return;
	}

	VoiceRecorderPtr recorder(g.sh->recorder);
	if (!recorder) {
		reset();
		return;
	}

	recorder->stop();
}

void VoiceRecorderDialog::on_qtTimer_timeout() {
	if (!g.sh) {
		reset();
		return;
	}

	if (!g.uiSession) {
		reset(false);
		return;
	}

	VoiceRecorderPtr recorder(g.sh->recorder);
	if (!g.sh->recorder) {
		reset();
		return;
	}

	QTime t, n;
	n = t.addMSecs(recorder->getElapsedTime() / 1000);

	qlTime->setText(n.toString(QLatin1String("hh:mm:ss")));
}

void VoiceRecorderDialog::on_qpbTargetDirectoryBrowse_clicked() {
	QString dir = QFileDialog::getExistingDirectory(this,
	              tr("Select target directory"),
	              QString(),
	              QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (!dir.isEmpty())
		qleTargetDirectory->setText(dir);
}

void VoiceRecorderDialog::reset(bool resettimer) {
	qtTimer->stop();

	if (g.sh) {
		VoiceRecorderPtr recorder(g.sh->recorder);
		if (recorder) {
			g.sh->recorder.reset();
			g.sh->announceRecordingState(false);
		}
	}

	qpbStart->setEnabled(true);
	qpbStop->setDisabled(true);

	qgbMode->setEnabled(true);
	qgbOutput->setEnabled(true);

	if (resettimer)
		qlTime->setText(QLatin1String("00:00:00"));
}

void VoiceRecorderDialog::onRecorderStopped() {
	reset(false);
}

void VoiceRecorderDialog::onRecorderStarted() {
	qlTime->setText(QLatin1String("00:00:00"));
	qtTimer->start();
}

void VoiceRecorderDialog::onRecorderError(int err, QString strerr) {
	Q_UNUSED(err);
	QMessageBox::critical(this,
	                      tr("Recorder"),
	                      strerr);
	reset(false);
}

