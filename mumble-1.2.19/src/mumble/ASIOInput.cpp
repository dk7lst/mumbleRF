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

#include "ASIOInput.h"

#include "MainWindow.h"
#include "Global.h"

class ASIOAudioInputRegistrar : public AudioInputRegistrar {
	public:
		ASIOAudioInputRegistrar();
		virtual AudioInput *create();
		virtual const QList<audioDevice> getDeviceChoices();
		virtual void setDeviceChoice(const QVariant &, Settings &);
		virtual bool canEcho(const QString &) const;

};

ASIOAudioInputRegistrar::ASIOAudioInputRegistrar() : AudioInputRegistrar(QLatin1String("ASIO")) {
}

AudioInput *ASIOAudioInputRegistrar::create() {
	return new ASIOInput();
}
const QList<audioDevice> ASIOAudioInputRegistrar::getDeviceChoices() {
	QList<audioDevice> qlReturn;
	return qlReturn;
}

bool ASIOAudioInputRegistrar::canEcho(const QString &) const {
	return true;
}

void ASIOAudioInputRegistrar::setDeviceChoice(const QVariant &, Settings &) {
}

static ConfigWidget *ASIOConfigDialogNew(Settings &st) {
	return new ASIOConfig(st);
}

class ASIOInit : public DeferInit {
		ASIOAudioInputRegistrar *airASIO;
		ConfigRegistrar *crASIO;
	public:
		ASIOInit() : airASIO(NULL), crASIO(NULL) {}
		void initialize();
		void destroy();
};

void ASIOInit::initialize() {
	HKEY hkDevs;
	HKEY hk;
	FILETIME ft;

	airASIO = NULL;
	crASIO = NULL;

	bool bFound = false;

	if (!g.s.bASIOEnable) {
		qWarning("ASIOInput: ASIO forcefully disabled via 'asio/enable' config option.");
		return;
	}

	// List of devices known to misbehave or be totally useless
	QStringList blacklist;
	blacklist << QLatin1String("{a91eaba1-cf4c-11d3-b96a-00a0c9c7b61a}"); // ASIO DirectX
	blacklist << QLatin1String("{e3186861-3a74-11d1-aef8-0080ad153287}"); // ASIO Multimedia
#ifdef QT_NO_DEBUG
	blacklist << QLatin1String("{232685c6-6548-49d8-846d-4141a3ef7560}"); // ASIO4ALL
#endif
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\ASIO", 0, KEY_READ, &hkDevs) == ERROR_SUCCESS) {
		DWORD idx = 0;
		DWORD keynamelen = 255;
		WCHAR keyname[255];
		while (RegEnumKeyEx(hkDevs, idx++, keyname, &keynamelen, NULL, NULL, NULL, &ft)  == ERROR_SUCCESS) {
			QString name=QString::fromUtf16(reinterpret_cast<ushort *>(keyname),keynamelen);
			if (RegOpenKeyEx(hkDevs, keyname, 0, KEY_READ, &hk) == ERROR_SUCCESS) {
				DWORD dtype = REG_SZ;
				WCHAR wclsid[255];
				DWORD datasize = 255;
				CLSID clsid;
				if (RegQueryValueEx(hk, L"CLSID", 0, &dtype, reinterpret_cast<BYTE *>(wclsid), &datasize) == ERROR_SUCCESS) {
					if (datasize > 76)
						datasize = 76;
					QString qsCls = QString::fromUtf16(reinterpret_cast<ushort *>(wclsid), datasize / 2).toLower().trimmed();
					if (! blacklist.contains(qsCls.toLower()) && ! FAILED(CLSIDFromString(wclsid, &clsid))) {
						bFound = true;
					}
				}
				RegCloseKey(hk);
			}
			keynamelen = 255;
		}
		RegCloseKey(hkDevs);
	}

	if (bFound) {
		airASIO = new ASIOAudioInputRegistrar();
		crASIO = new ConfigRegistrar(2002, ASIOConfigDialogNew);
	} else {
		qWarning("ASIO: No valid devices found, disabling");
	}
}

void ASIOInit::destroy() {
	delete airASIO;
	delete crASIO;
}

static class ASIOInit asioinit;

ASIOInput *ASIOInput::aiSelf;

ASIOConfig::ASIOConfig(Settings &st) : ConfigWidget(st) {
	HKEY hkDevs;
	HKEY hk;
	DWORD idx = 0;
	WCHAR keyname[255];
	DWORD keynamelen = 255;
	FILETIME ft;
	HRESULT hr;

	setupUi(this);

	// List of devices known to misbehave or be totally useless
	QStringList blacklist;
	blacklist << QLatin1String("{a91eaba1-cf4c-11d3-b96a-00a0c9c7b61a}"); // ASIO DirectX
	blacklist << QLatin1String("{e3186861-3a74-11d1-aef8-0080ad153287}"); // ASIO Multimedia
#ifdef QT_NO_DEBUG
	blacklist << QLatin1String("{232685c6-6548-49d8-846d-4141a3ef7560}"); // ASIO4ALL
#endif
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\ASIO", 0, KEY_READ, &hkDevs) == ERROR_SUCCESS) {
		while (RegEnumKeyEx(hkDevs, idx++, keyname, &keynamelen, NULL, NULL, NULL, &ft)  == ERROR_SUCCESS) {
			QString name=QString::fromUtf16(reinterpret_cast<ushort *>(keyname),keynamelen);
			if (RegOpenKeyEx(hkDevs, keyname, 0, KEY_READ, &hk) == ERROR_SUCCESS) {
				DWORD dtype = REG_SZ;
				WCHAR wclsid[255];
				DWORD datasize = 255;
				CLSID clsid;
				if (RegQueryValueEx(hk, L"CLSID", 0, &dtype, reinterpret_cast<BYTE *>(wclsid), &datasize) == ERROR_SUCCESS) {
					if (datasize > 76)
						datasize = 76;
					QString qsCls = QString::fromUtf16(reinterpret_cast<ushort *>(wclsid), datasize / 2).toLower().trimmed();
					if (! blacklist.contains(qsCls) && ! FAILED(hr =CLSIDFromString(wclsid, &clsid))) {
						ASIODev ad(name, qsCls);
						qlDevs << ad;
					}
				}
				RegCloseKey(hk);
			}
			keynamelen = 255;
		}
		RegCloseKey(hkDevs);
	}

	bOk = false;

	ASIODev ad;

	foreach(ad, qlDevs) {
		qcbDevice->addItem(ad.first, QVariant(ad.second));
	}

	if (qlDevs.count() == 0) {
		qpbQuery->setEnabled(false);
		qpbConfig->setEnabled(false);
	}
}

void ASIOConfig::on_qpbQuery_clicked() {
	QString qsCls = qcbDevice->itemData(qcbDevice->currentIndex()).toString();
	CLSID clsid;
	IASIO *iasio;

	clearQuery();

	CLSIDFromString(const_cast<wchar_t *>(reinterpret_cast<const wchar_t *>(qsCls.utf16())), &clsid);
	if (CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, clsid, reinterpret_cast<void **>(&iasio)) == S_OK) {
		SleepEx(10, false);
		if (iasio->init(winId())) {
			SleepEx(10, false);
			char buff[512];
			memset(buff, 0, 512);

			iasio->getDriverName(buff);
			SleepEx(10, false);

			long ver = iasio->getDriverVersion();
			SleepEx(10, false);

			ASIOSampleRate srate = 0.0;
			iasio->setSampleRate(48000.0);
			iasio->getSampleRate(&srate);
			SleepEx(10, false);

			long minSize, maxSize, prefSize, granSize;
			iasio->getBufferSize(&minSize, &maxSize, &prefSize, &granSize);
			SleepEx(10, false);

			QString str = tr("%1 (version %2)").arg(QLatin1String(buff)).arg(ver);
			qlName->setText(str);

			str = tr("%1 -> %2 samples buffer, with %3 sample resolution (%4 preferred) at %5 Hz").arg(minSize).arg(maxSize).arg(granSize).arg(prefSize).arg(srate,0,'f',0);

			qlBuffers->setText(str);

			long ichannels, ochannels;
			iasio->getChannels(&ichannels, &ochannels);
			SleepEx(10, false);
			long cnum;

			bool match = (s.qsASIOclass == qsCls);

			for (cnum=0;cnum<ichannels;cnum++) {
				ASIOChannelInfo aci;
				aci.channel = cnum;
				aci.isInput = true;
				iasio->getChannelInfo(&aci);
				SleepEx(10, false);
				switch (aci.type) {
					case ASIOSTFloat32LSB:
					case ASIOSTInt32LSB:
					case ASIOSTInt24LSB:
					case ASIOSTInt16LSB: {
							QListWidget *widget = qlwUnused;
							QVariant v = static_cast<int>(cnum);
							if (match && s.qlASIOmic.contains(v))
								widget = qlwMic;
							else if (match && s.qlASIOspeaker.contains(v))
								widget = qlwSpeaker;
							QListWidgetItem *item = new QListWidgetItem(QLatin1String(aci.name), widget);
							item->setData(Qt::UserRole, static_cast<int>(cnum));
						}
						break;
					default:
						qWarning("ASIOInput: Channel %ld %s (Unusable format %ld)", cnum, aci.name,aci.type);
				}
			}

			bOk = true;
		} else {
			SleepEx(10, false);
			char err[255];
			iasio->getErrorMessage(err);
			SleepEx(10, false);
			QMessageBox::critical(this, QLatin1String("Mumble"), tr("ASIO Initialization failed: %1").arg(Qt::escape(QLatin1String(err))), QMessageBox::Ok, QMessageBox::NoButton);
		}
		iasio->Release();
	} else {
		QMessageBox::critical(this, QLatin1String("Mumble"), tr("Failed to instantiate ASIO driver"), QMessageBox::Ok, QMessageBox::NoButton);
	}
}

void ASIOConfig::on_qpbConfig_clicked() {
	QString qsCls = qcbDevice->itemData(qcbDevice->currentIndex()).toString();
	CLSID clsid;
	IASIO *iasio;

	CLSIDFromString(const_cast<wchar_t *>(reinterpret_cast<const wchar_t *>(qsCls.utf16())), &clsid);
	if (CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, clsid, reinterpret_cast<void **>(&iasio)) == S_OK) {
		SleepEx(10, false);
		if (iasio->init(winId())) {
			SleepEx(10, false);
			iasio->controlPanel();
			SleepEx(10, false);
		} else {
			SleepEx(10, false);
			char err[255];
			iasio->getErrorMessage(err);
			SleepEx(10, false);
			QMessageBox::critical(this, QLatin1String("Mumble"), tr("ASIO Initialization failed: %1").arg(Qt::escape(QLatin1String(err))), QMessageBox::Ok, QMessageBox::NoButton);
		}
		iasio->Release();
	} else {
		QMessageBox::critical(this, QLatin1String("Mumble"), tr("Failed to instantiate ASIO driver"), QMessageBox::Ok, QMessageBox::NoButton);
	}
}

void ASIOConfig::on_qcbDevice_activated(int) {
	clearQuery();
}

void ASIOConfig::on_qpbAddMic_clicked() {
	int row = qlwUnused->currentRow();
	if (row < 0)
		return;
	qlwMic->addItem(qlwUnused->takeItem(row));
}

void ASIOConfig::on_qpbRemMic_clicked() {
	int row = qlwMic->currentRow();
	if (row < 0)
		return;
	qlwUnused->addItem(qlwMic->takeItem(row));
}

void ASIOConfig::on_qpbAddSpeaker_clicked() {
	int row = qlwUnused->currentRow();
	if (row < 0)
		return;
	qlwSpeaker->addItem(qlwUnused->takeItem(row));
}

void ASIOConfig::on_qpbRemSpeaker_clicked() {
	int row = qlwSpeaker->currentRow();
	if (row < 0)
		return;
	qlwUnused->addItem(qlwSpeaker->takeItem(row));
}

QString ASIOConfig::title() const {
	return tr("ASIO");
}

QIcon ASIOConfig::icon() const {
	return QIcon(QLatin1String("skin:config_asio.png"));
}

void ASIOConfig::save() const {
	if (! bOk)
		return;

	s.qsASIOclass = qcbDevice->itemData(qcbDevice->currentIndex()).toString();

	QList<QVariant> list;

	for (int i=0;i<qlwMic->count();i++) {
		QListWidgetItem *item = qlwMic->item(i);
		list << item->data(Qt::UserRole);
	}

	s.qlASIOmic = list;

	list.clear();

	for (int i=0;i<qlwSpeaker->count();i++) {
		QListWidgetItem *item = qlwSpeaker->item(i);
		list << item->data(Qt::UserRole);
	}

	s.qlASIOspeaker = list;
}

void ASIOConfig::load(const Settings &r) {
	int i = 0;
	ASIODev ad;
	foreach(ad, qlDevs) {
		if (ad.second == r.qsASIOclass) {
			loadComboBox(qcbDevice, i);
		}
		i++;
	}
	s.qlASIOmic = r.qlASIOmic;
	s.qlASIOspeaker = r.qlASIOspeaker;

	qlName->setText(QString());
	qlBuffers->setText(QString());
	qlwMic->clear();
	qlwUnused->clear();
	qlwSpeaker->clear();
}

bool ASIOConfig::expert(bool) {
	return false;
}

void ASIOConfig::clearQuery() {
	bOk = false;
	qlName->setText(QString());
	qlBuffers->setText(QString());
	qlwMic->clear();
	qlwUnused->clear();
	qlwSpeaker->clear();
}

ASIOInput::ASIOInput() {
	QString qsCls = g.s.qsASIOclass;
	CLSID clsid;

	iasio = NULL;
	abiInfo = NULL;
	aciInfo = NULL;

	// Sanity check things first.

	iNumMic=g.s.qlASIOmic.count();
	iNumSpeaker=g.s.qlASIOspeaker.count();

	if ((iNumMic == 0) || (iNumSpeaker == 0)) {
		QMessageBox::warning(NULL, QLatin1String("Mumble"), tr("You need to select at least one microphone and one speaker source to use ASIO. "
		                     "If you just need microphone sampling, use DirectSound."),  QMessageBox::Ok, QMessageBox::NoButton);
		return;
	}

	CLSIDFromString(const_cast<wchar_t *>(reinterpret_cast<const wchar_t *>(qsCls.utf16())), &clsid);
	if (CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, clsid, reinterpret_cast<void **>(&iasio)) == S_OK) {
		if (iasio->init(NULL)) {
			iasio->setSampleRate(48000.0);
			ASIOSampleRate srate = 0.0;
			iasio->getSampleRate(&srate);

			if (srate <= 0.0)
				return;

			long minSize, maxSize, prefSize, granSize;
			iasio->getBufferSize(&minSize, &maxSize, &prefSize, &granSize);

			bool halfit = true;

			double wbuf = (srate / 100.0);
			long wantBuf = lround(wbuf);
			lBufSize = wantBuf;

			if (static_cast<double>(wantBuf) == wbuf) {
				qWarning("ASIOInput: Exact buffer match possible.");
				if ((wantBuf >= minSize) && (wantBuf <= maxSize)) {
					if (wantBuf == minSize)
						halfit = false;
					else if ((granSize >= 1) && (((wantBuf-minSize)%granSize)==0))
						halfit = false;
				}
			}

			if (halfit) {
				if (granSize == 0) {
					qWarning("ASIOInput: Single buffer size");
					lBufSize = minSize;
				} else {
					long target = wantBuf / 2;
					lBufSize = target;
					while (lBufSize < target) {
						if (granSize < 0)
							lBufSize *= 2;
						else
							lBufSize += granSize;
					}
				}
				qWarning("ASIOInput: Buffer mismatch mode. Wanted %d, got %d", wantBuf, lBufSize);
			}


			abiInfo = new ASIOBufferInfo[iNumMic + iNumSpeaker];
			aciInfo = new ASIOChannelInfo[iNumMic + iNumSpeaker];

			int i, idx = 0;
			for (i=0;i<iNumMic;i++) {
				abiInfo[idx].isInput = true;
				abiInfo[idx].channelNum = g.s.qlASIOmic[i].toInt();

				aciInfo[idx].channel = abiInfo[idx].channelNum;
				aciInfo[idx].isInput = true;
				iasio->getChannelInfo(&aciInfo[idx]);
				SleepEx(10, false);

				idx++;
			}
			for (i=0;i<iNumSpeaker;i++) {
				abiInfo[idx].isInput = true;
				abiInfo[idx].channelNum = g.s.qlASIOspeaker[i].toInt();

				aciInfo[idx].channel = abiInfo[idx].channelNum;
				aciInfo[idx].isInput = true;
				iasio->getChannelInfo(&aciInfo[idx]);
				SleepEx(10, false);

				idx++;
			}

			iEchoChannels = iNumSpeaker;
			iMicChannels = iNumMic;
			iEchoFreq = iMicFreq = iroundf(srate);

			initializeMixer();

			ASIOCallbacks asioCallbacks;
			ZeroMemory(&asioCallbacks, sizeof(asioCallbacks));
			asioCallbacks.bufferSwitch = &bufferSwitch;
			asioCallbacks.sampleRateDidChange = &sampleRateChanged;
			asioCallbacks.asioMessage = &asioMessages;
			asioCallbacks.bufferSwitchTimeInfo = &bufferSwitchTimeInfo;

			if (iasio->createBuffers(abiInfo, idx, lBufSize, &asioCallbacks) == ASE_OK) {
				bRunning = true;
				return;
			}
		}
	}

	if (iasio) {
		iasio->Release();
		iasio = NULL;
	}

	QMessageBox::critical(NULL, QLatin1String("Mumble"), tr("Opening selected ASIO device failed. No input will be done."),
	                      QMessageBox::Ok, QMessageBox::NoButton);

}

ASIOInput::~ASIOInput() {
	qwDone.wakeAll();
	wait();
	if (iasio) {
		iasio->stop();
		iasio->disposeBuffers();
		iasio->Release();
		iasio = NULL;
	}

	delete [] abiInfo;
	abiInfo = NULL;

	delete [] aciInfo;
	aciInfo = NULL;
}

void ASIOInput::run() {
	QMutex m;
	m.lock();
	if (iasio) {
		aiSelf = this;
		iasio->start();
		qwDone.wait(&m);
	}
}

ASIOTime *ASIOInput::bufferSwitchTimeInfo(ASIOTime *, long index, ASIOBool) {
	aiSelf->bufferReady(index);
	return 0L;
}

void
ASIOInput::addBuffer(ASIOSampleType sampType, int interleave, void *src, float * RESTRICT dst) {
	switch (sampType) {
		case ASIOSTInt16LSB: {
				const float m = 1.0f / 32768.f;
				const short * RESTRICT buf=static_cast<short *>(src);
				for (int i=0;i<lBufSize;i++)
					dst[i*interleave]=buf[i] * m;
			}
			break;
		case ASIOSTInt32LSB: {
				const float m = 1.0f / 2147483648.f;
				const int * RESTRICT buf=static_cast<int *>(src);
				for (int i=0;i<lBufSize;i++)
					dst[i*interleave]=buf[i] * m;
			}
			break;
		case ASIOSTInt24LSB: {
				const float m = 1.0f / static_cast<float>(0x7FFFFFFF - 0xFF);
				const unsigned char * RESTRICT buf=static_cast<unsigned char *>(src);
				for (int i=0;i<lBufSize;i++)
					dst[i * interleave] = (buf[i*3] << 24 | buf[i*3+1] << 16 | buf[i*3+2] << 8) * m;
			}
			break;
		case ASIOSTFloat32LSB: {
				const float * RESTRICT buf=static_cast<float *>(src);
				for (int i=0;i<lBufSize;i++)
					dst[i*interleave]=buf[i];
			}
			break;
	}
}

void
ASIOInput::bufferReady(long buffindex) {
	STACKVAR(float, buffer, lBufSize * qMax(iNumMic,iNumSpeaker));

	for (int c=0;c<iNumSpeaker;++c)
		addBuffer(aciInfo[iNumMic+c].type, iNumSpeaker, abiInfo[iNumMic+c].buffers[buffindex], buffer+c);
	addEcho(buffer, lBufSize);

	for (int c=0;c<iNumMic;++c)
		addBuffer(aciInfo[c].type, iNumMic, abiInfo[c].buffers[buffindex], buffer+c);
	addMic(buffer, lBufSize);
}

void ASIOInput::bufferSwitch(long index, ASIOBool processNow) {
	ASIOTime  timeInfo;
	memset(&timeInfo, 0, sizeof(timeInfo));

	if (aiSelf->iasio->getSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime) == ASE_OK)
		timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

	bufferSwitchTimeInfo(&timeInfo, index, processNow);
}

void ASIOInput::sampleRateChanged(ASIOSampleRate) {
	qFatal("ASIOInput: sampleRateChanged");
}

long ASIOInput::asioMessages(long selector, long value, void*, double*) {
	long ret = 0;
	switch (selector) {
		case kAsioSelectorSupported:
			if (value == kAsioResetRequest
			        || value == kAsioEngineVersion
			        || value == kAsioResyncRequest
			        || value == kAsioLatenciesChanged
			        || value == kAsioSupportsTimeInfo
			        || value == kAsioSupportsTimeCode
			        || value == kAsioSupportsInputMonitor)
				ret = 1L;
			break;
		case kAsioResetRequest:
			qFatal("ASIOInput: kAsioResetRequest");
			ret = 1L;
			break;
		case kAsioResyncRequest:
			ret = 1L;
			break;
		case kAsioLatenciesChanged:
			ret = 1L;
			break;
		case kAsioEngineVersion:
			ret = 2L;
			break;
		case kAsioSupportsTimeInfo:
			ret = 1;
			break;
		case kAsioSupportsTimeCode:
			ret = 0;
			break;
	}
	return ret;
}
