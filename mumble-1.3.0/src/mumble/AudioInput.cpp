// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "AudioInput.h"

#include "AudioOutput.h"
#include "CELTCodec.h"
#include "OpusCodec.h"
#include "ServerHandler.h"
#include "MainWindow.h"
#include "User.h"
#include "PacketDataStream.h"
#include "Plugins.h"
#include "Message.h"
#include "Global.h"
#include "NetworkConfig.h"
#include "VoiceRecorder.h"
#include "ExternalPTT.h"

#ifdef USE_RNNOISE
extern "C" {
#include "rnnoise.h"
}
#endif

// Remember that we cannot use static member classes that are not pointers, as the constructor
// for AudioInputRegistrar() might be called before they are initialized, as the constructor
// is called from global initialization.
// Hence, we allocate upon first call.

QMap<QString, AudioInputRegistrar *> *AudioInputRegistrar::qmNew;
QString AudioInputRegistrar::current = QString();

AudioInputRegistrar::AudioInputRegistrar(const QString &n, int p) : name(n), priority(p) {
	if (! qmNew)
		qmNew = new QMap<QString, AudioInputRegistrar *>();
	qmNew->insert(name,this);
}

AudioInputRegistrar::~AudioInputRegistrar() {
	qmNew->remove(name);
}

AudioInputPtr AudioInputRegistrar::newFromChoice(QString choice) {
	if (! qmNew)
		return AudioInputPtr();

	if (!choice.isEmpty() && qmNew->contains(choice)) {
		g.s.qsAudioInput = choice;
		current = choice;
		return AudioInputPtr(qmNew->value(current)->create());
	}
	choice = g.s.qsAudioInput;
	if (qmNew->contains(choice)) {
		current = choice;
		return AudioInputPtr(qmNew->value(choice)->create());
	}

	AudioInputRegistrar *r = NULL;
	foreach(AudioInputRegistrar *air, *qmNew)
		if (!r || (air->priority > r->priority))
			r = air;
	if (r) {
		current = r->name;
		return AudioInputPtr(r->create());
	}
	return AudioInputPtr();
}

bool AudioInputRegistrar::canExclusive() const {
	return false;
}

AudioInput::AudioInput() : opusBuffer(g.s.iFramesPerPacket * (SAMPLE_RATE / 100)) {
	adjustBandwidth(g.iMaxBandwidth, iAudioQuality, iAudioFrames);

	g.iAudioBandwidth = getNetworkBandwidth(iAudioQuality, iAudioFrames);

	umtType = MessageHandler::UDPVoiceCELTAlpha;

	activityState = ActivityStateActive;
	oCodec = NULL;
	opusState = NULL;
	cCodec = NULL;
	ceEncoder = NULL;

	iSampleRate = SAMPLE_RATE;
	iFrameSize = SAMPLE_RATE / 100;

#ifdef USE_OPUS
	oCodec = g.oCodec;
	if (oCodec) {
		if (!g.s.bUseOpusMusicEncoding) {
			opusState = oCodec->opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, NULL);
			qWarning("AudioInput: Opus encoder set for VOIP");
		} else {
			opusState = oCodec->opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_AUDIO, NULL);
			qWarning("AudioInput: Opus encoder set for Music");
		}

		oCodec->opus_encoder_ctl(opusState, OPUS_SET_VBR(0)); // CBR
	}
#endif

#ifdef USE_RNNOISE
	denoiseState = rnnoise_create();
#endif

	qWarning("AudioInput: %d bits/s, %d hz, %d sample", iAudioQuality, iSampleRate, iFrameSize);
	iEchoFreq = iMicFreq = iSampleRate;

	iFrameCounter = 0;
	iSilentFrames = 0;
	iHoldFrames = 0;
	iBufferedFrames = 0;

	bResetProcessor = true;

	bEchoMulti = false;

	sppPreprocess = NULL;
	sesEcho = NULL;
	srsMic = srsEcho = NULL;
	iJitterSeq = 0;
	iMinBuffered = 1000;

	psMic = new short[iFrameSize];
	psClean = new short[iFrameSize];

	psSpeaker = NULL;

	iEchoChannels = iMicChannels = 0;
	iEchoFilled = iMicFilled = 0;
	eMicFormat = eEchoFormat = SampleFloat;
	iMicSampleSize = iEchoSampleSize = 0;

	bPreviousVoice = false;

	bResetEncoder = true;

	pfMicInput = pfEchoInput = pfOutput = NULL;

	iBitrate = 0;
	dPeakSignal = dPeakSpeaker = dPeakMic = dPeakCleanMic = 0.0;

	if (g.uiSession) {
		setMaxBandwidth(g.iMaxBandwidth);
	}

	bRunning = true;

	connect(this, SIGNAL(doDeaf()), g.mw->qaAudioDeaf, SLOT(trigger()), Qt::QueuedConnection);
	connect(this, SIGNAL(doMute()), g.mw->qaAudioMute, SLOT(trigger()), Qt::QueuedConnection);
}

AudioInput::~AudioInput() {
	bRunning = false;
	wait();

#ifdef USE_OPUS
	if (opusState) {
		oCodec->opus_encoder_destroy(opusState);
	}
#endif

#ifdef USE_RNNOISE
	if (denoiseState) {
		rnnoise_destroy(denoiseState);
	}
#endif

	if (ceEncoder) {
		cCodec->celt_encoder_destroy(ceEncoder);
	}

	foreach(short *buf, qlEchoFrames)
		delete [] buf;

	if (sppPreprocess)
		speex_preprocess_state_destroy(sppPreprocess);
	if (sesEcho)
		speex_echo_state_destroy(sesEcho);

	if (srsMic)
		speex_resampler_destroy(srsMic);
	if (srsEcho)
		speex_resampler_destroy(srsEcho);

	delete [] psMic;
	delete [] psClean;
	delete [] psSpeaker;

	delete [] pfMicInput;
	delete [] pfEchoInput;
	delete [] pfOutput;
}

bool AudioInput::isTransmitting() const {
	return bPreviousVoice;
};

#define IN_MIXER_FLOAT(channels) \
static void inMixerFloat##channels ( float * RESTRICT buffer, const void * RESTRICT ipt, unsigned int nsamp, unsigned int N, quint64 mask) { \
  const float * RESTRICT input = reinterpret_cast<const float *>(ipt); \
  const float m = 1.0f / static_cast<float>(channels); \
  Q_UNUSED(N); \
  Q_UNUSED(mask); \
  for(unsigned int i=0;i<nsamp;++i) {\
	  float v= 0.0f; \
	  for(unsigned int j=0;j<channels;++j) \
	  	v += input[i*channels+j]; \
	  buffer[i] = v * m; \
  } \
}

#define IN_MIXER_SHORT(channels) \
static void inMixerShort##channels ( float * RESTRICT buffer, const void * RESTRICT ipt, unsigned int nsamp, unsigned int N, quint64 mask) { \
  const short * RESTRICT input = reinterpret_cast<const short *>(ipt); \
  const float m = 1.0f / (32768.f * static_cast<float>(channels)); \
  Q_UNUSED(N); \
  Q_UNUSED(mask); \
  for(unsigned int i=0;i<nsamp;++i) {\
	  float v= 0.0f; \
	  for(unsigned int j=0;j<channels;++j) \
	  	v += static_cast<float>(input[i*channels+j]); \
	  buffer[i] = v * m; \
  } \
}

static void inMixerFloatMask(float * RESTRICT buffer, const void * RESTRICT ipt, unsigned int nsamp, unsigned int N, quint64 mask) { \
	const float * RESTRICT input = reinterpret_cast<const float *>(ipt);

	unsigned int chancount = 0;
	STACKVAR(unsigned int, chanindex, N);
	for (unsigned int j = 0; j < N; ++j) {
		if ((mask & (1ULL << j)) == 0) {
			continue;
		}
		chanindex[chancount] = j; // Use chancount as index into chanindex.
		++chancount;
	}

	const float m = 1.0f / static_cast<float>(chancount);
	for(unsigned int i = 0; i < nsamp; ++i) {
		float v = 0.0f;
		for(unsigned int j = 0; j < chancount; ++j) {
			v += input[i * N + chanindex[j]];
		}
		buffer[i] = v * m;
	}
}

static void inMixerShortMask(float * RESTRICT buffer, const void * RESTRICT ipt, unsigned int nsamp, unsigned int N, quint64 mask) {
	const short * RESTRICT input = reinterpret_cast<const short *>(ipt);

	unsigned int chancount = 0;
	STACKVAR(unsigned int, chanindex, N);
	for (unsigned int j = 0; j < N; ++j) {
		if ((mask & (1ULL << j)) == 0) {
			continue;
		}
		chanindex[chancount] = j; // Use chancount as index into chanindex.
		++chancount;
	}

	const float m = 1.0f / static_cast<float>(chancount);
	for(unsigned int i = 0; i < nsamp; ++i) {
		float v = 0.0f;
		for(unsigned int j = 0; j < chancount; ++j) {
			v += static_cast<float>(input[i * N + chanindex[j]]);
		}
		buffer[i] = v * m;
	}
}

IN_MIXER_FLOAT(1)
IN_MIXER_FLOAT(2)
IN_MIXER_FLOAT(3)
IN_MIXER_FLOAT(4)
IN_MIXER_FLOAT(5)
IN_MIXER_FLOAT(6)
IN_MIXER_FLOAT(7)
IN_MIXER_FLOAT(8)
IN_MIXER_FLOAT(N)

IN_MIXER_SHORT(1)
IN_MIXER_SHORT(2)
IN_MIXER_SHORT(3)
IN_MIXER_SHORT(4)
IN_MIXER_SHORT(5)
IN_MIXER_SHORT(6)
IN_MIXER_SHORT(7)
IN_MIXER_SHORT(8)
IN_MIXER_SHORT(N)

AudioInput::inMixerFunc AudioInput::chooseMixer(const unsigned int nchan, SampleFormat sf, quint64 chanmask) {
	inMixerFunc r = NULL;

	if (chanmask != 0xffffffffffffffffULL) {
		if (sf == SampleFloat) {
			r = inMixerFloatMask;
		} else if (sf == SampleShort) {
			r = inMixerShortMask;
		}
		return r;
	}

	if (sf == SampleFloat) {
		switch (nchan) {
			case 1:
				r = inMixerFloat1;
				break;
			case 2:
				r = inMixerFloat2;
				break;
			case 3:
				r = inMixerFloat3;
				break;
			case 4:
				r = inMixerFloat4;
				break;
			case 5:
				r = inMixerFloat5;
				break;
			case 6:
				r = inMixerFloat6;
				break;
			case 7:
				r = inMixerFloat7;
				break;
			case 8:
				r = inMixerFloat8;
				break;
			default:
				r = inMixerFloatN;
				break;
		}
	} else {
		switch (nchan) {
			case 1:
				r = inMixerShort1;
				break;
			case 2:
				r = inMixerShort2;
				break;
			case 3:
				r = inMixerShort3;
				break;
			case 4:
				r = inMixerShort4;
				break;
			case 5:
				r = inMixerShort5;
				break;
			case 6:
				r = inMixerShort6;
				break;
			case 7:
				r = inMixerShort7;
				break;
			case 8:
				r = inMixerShort8;
				break;
			default:
				r = inMixerShortN;
				break;
		}
	}
	return r;
}

void AudioInput::initializeMixer() {
	int err;

	if (srsMic)
		speex_resampler_destroy(srsMic);
	if (srsEcho)
		speex_resampler_destroy(srsEcho);
	delete [] pfMicInput;
	delete [] pfEchoInput;
	delete [] pfOutput;

	if (iMicFreq != iSampleRate)
		srsMic = speex_resampler_init(1, iMicFreq, iSampleRate, 3, &err);

	iMicLength = (iFrameSize * iMicFreq) / iSampleRate;

	pfMicInput = new float[iMicLength];
	pfOutput = new float[iFrameSize * qMax(1U,iEchoChannels)];

	if (iEchoChannels > 0) {
		bEchoMulti = g.s.bEchoMulti;
		if (iEchoFreq != iSampleRate)
			srsEcho = speex_resampler_init(bEchoMulti ? iEchoChannels : 1, iEchoFreq, iSampleRate, 3, &err);
		iEchoLength = (iFrameSize * iEchoFreq) / iSampleRate;
		iEchoMCLength = bEchoMulti ? iEchoLength * iEchoChannels : iEchoLength;
		iEchoFrameSize = bEchoMulti ? iFrameSize * iEchoChannels : iFrameSize;
		pfEchoInput = new float[iEchoMCLength];
	} else {
		srsEcho = NULL;
		pfEchoInput = NULL;
	}

	uiMicChannelMask = g.s.uiAudioInputChannelMask;

	// There is no channel mask setting for the echo canceller, so allow all channels.
	uiEchoChannelMask = 0xffffffffffffffffULL;

	imfMic = chooseMixer(iMicChannels, eMicFormat, uiMicChannelMask);
	imfEcho = chooseMixer(iEchoChannels, eEchoFormat, uiEchoChannelMask);

	iMicSampleSize = static_cast<int>(iMicChannels * ((eMicFormat == SampleFloat) ? sizeof(float) : sizeof(short)));
	iEchoSampleSize = static_cast<int>(iEchoChannels * ((eEchoFormat == SampleFloat) ? sizeof(float) : sizeof(short)));

	bResetProcessor = true;

	qWarning("AudioInput: Initialized mixer for %d channel %d hz mic and %d channel %d hz echo", iMicChannels, iMicFreq, iEchoChannels, iEchoFreq);
	if (uiMicChannelMask != 0xffffffffffffffffULL) {
		qWarning("AudioInput: using mic channel mask 0x%llx", static_cast<unsigned long long>(uiMicChannelMask));
	}
}

void AudioInput::addMic(const void *data, unsigned int nsamp) {
	while (nsamp > 0) {
		// Make sure we don't overrun the frame buffer
		const unsigned int left = qMin(nsamp, iMicLength - iMicFilled);

		// Append mix into pfMicInput frame buffer (converts 16bit pcm->float if necessary)
		imfMic(pfMicInput + iMicFilled, data, left, iMicChannels, uiMicChannelMask);

		iMicFilled += left;
		nsamp -= left;

		// If new samples are left offset data pointer to point at the first one for next iteration
		if (nsamp > 0) {
			if (eMicFormat == SampleFloat)
				data = reinterpret_cast<const float *>(data) + left * iMicChannels;
			else
				data = reinterpret_cast<const short *>(data) + left * iMicChannels;
		}

		if (iMicFilled == iMicLength) {
			// Frame complete
			iMicFilled = 0;

			// If needed resample frame
			float *ptr = srsMic ? pfOutput : pfMicInput;

			if (srsMic) {
				spx_uint32_t inlen = iMicLength;
				spx_uint32_t outlen = iFrameSize;
				speex_resampler_process_float(srsMic, 0, pfMicInput, &inlen, pfOutput, &outlen);
			}

			// Convert float to 16bit PCM
			const float mul = 32768.f;
			for (int j = 0; j < iFrameSize; ++j)
				psMic[j] = static_cast<short>(qBound(-32768.f, (ptr[j] * mul), 32767.f));

			// If we have echo chancellation enabled...
			if (iEchoChannels > 0) {
				short *echo = NULL;

				{
					QMutexLocker l(&qmEcho);

					if (qlEchoFrames.isEmpty()) {
						iJitterSeq = 0;
						iMinBuffered = 1000;
					} else {
						// Compensate for drift between the microphone and the echo source
						iMinBuffered = qMin(iMinBuffered, qlEchoFrames.count());

						if ((iJitterSeq > 100) && (iMinBuffered > 1)) {
							iJitterSeq = 0;
							iMinBuffered = 1000;
							delete [] qlEchoFrames.takeFirst();
						}
						echo = qlEchoFrames.takeFirst();
					}
				}

				if (echo) {
					// We have echo data for the current frame, remember that
					delete [] psSpeaker;
					psSpeaker = echo;
				}
			}

			// Encode and send frame
			encodeAudioFrame();
		}
	}
}

void AudioInput::addEcho(const void *data, unsigned int nsamp) {
	while (nsamp > 0) {
		// Make sure we don't overrun the echo frame buffer
		const unsigned int left = qMin(nsamp, iEchoLength - iEchoFilled);

		if (bEchoMulti) {
			const unsigned int samples = left * iEchoChannels;

			if (eEchoFormat == SampleFloat) {
				for (unsigned int i=0;i<samples;++i)
					pfEchoInput[i] = reinterpret_cast<const float *>(data)[i];
			}
			else {
				// 16bit PCM -> float
				for (unsigned int i=0;i<samples;++i)
					pfEchoInput[i] = static_cast<float>(reinterpret_cast<const short *>(data)[i]) * (1.0f / 32768.f);
			}
		} else {
			// Mix echo channels (converts 16bit PCM -> float if needed)
			imfEcho(pfEchoInput + iEchoFilled, data, left, iEchoChannels, uiEchoChannelMask);
		}

		iEchoFilled += left;
		nsamp -= left;

		// If new samples are left offset data pointer to point at the first one for next iteration
		if (nsamp > 0) {
			if (eEchoFormat == SampleFloat)
				data = reinterpret_cast<const float *>(data) + left * iEchoChannels;
			else
				data = reinterpret_cast<const short *>(data) + left * iEchoChannels;
		}

		if (iEchoFilled == iEchoLength) {
			//Frame complete

			iEchoFilled = 0;

			// Resample if necessary
			float *ptr = srsEcho ? pfOutput : pfEchoInput;

			if (srsEcho) {
				spx_uint32_t inlen = iEchoLength;
				spx_uint32_t outlen = iFrameSize;
				speex_resampler_process_interleaved_float(srsEcho, pfEchoInput, &inlen, pfOutput, &outlen);
			}

			short *outbuff = new short[iEchoFrameSize];

			// float -> 16bit PCM
			const float mul = 32768.f;
			for (unsigned int j=0;j<iEchoFrameSize;++j)
				outbuff[j] = static_cast<short>(ptr[j] * mul);

			// Push frame into the echo chancellers jitter buffer
			QMutexLocker l(&qmEcho);

			iJitterSeq = qMin(iJitterSeq + 1,10000U);
			qlEchoFrames.append(outbuff);
		}
	}
}

void AudioInput::adjustBandwidth(int bitspersec, int &bitrate, int &frames) {
	frames = g.s.iFramesPerPacket;
	bitrate = g.s.iQuality;

	if (bitspersec == -1) {
		// No limit
	} else {
		if (getNetworkBandwidth(bitrate, frames) > bitspersec) {
			if ((frames <= 4) && (bitspersec <= 32000))
				frames = 4;
			else if ((frames == 1) && (bitspersec <= 64000))
				frames = 2;
			else if ((frames == 2) && (bitspersec <= 48000))
				frames = 4;
			if (getNetworkBandwidth(bitrate, frames) > bitspersec) {
				do {
					bitrate -= 1000;
				} while ((bitrate > 8000) && (getNetworkBandwidth(bitrate, frames) > bitspersec));
			}
		}
	}
	if (bitrate <= 8000)
		bitrate = 8000;
}

void AudioInput::setMaxBandwidth(int bitspersec) {
	if (bitspersec == g.iMaxBandwidth)
		return;

	int frames;
	int bitrate;
	adjustBandwidth(bitspersec, bitrate, frames);

	g.iMaxBandwidth = bitspersec;

	if (bitspersec != -1) {
		if ((bitrate != g.s.iQuality) || (frames != g.s.iFramesPerPacket))
			g.mw->msgBox(tr("Server maximum network bandwidth is only %1 kbit/s. Audio quality auto-adjusted to %2 kbit/s (%3 ms)").arg(bitspersec / 1000).arg(bitrate / 1000).arg(frames*10));
	}

	AudioInputPtr ai = g.ai;
	if (ai) {
		g.iAudioBandwidth = getNetworkBandwidth(bitrate, frames);
		ai->iAudioQuality = bitrate;
		ai->iAudioFrames = frames;
		return;
	}

	ai.reset();

	Audio::stopInput();
	Audio::startInput();
}

int AudioInput::getNetworkBandwidth(int bitrate, int frames) {
	int overhead = 20 + 8 + 4 + 1 + 2 + (g.s.bTransmitPosition ? 12 : 0) + (NetworkConfig::TcpModeEnabled() ? 12 : 0) + frames;
	overhead *= (800 / frames);
	int bw = overhead + bitrate;

	return bw;
}

void AudioInput::resetAudioProcessor() {
	if (!bResetProcessor)
		return;

	int iArg;

	if (sppPreprocess)
		speex_preprocess_state_destroy(sppPreprocess);
	if (sesEcho)
		speex_echo_state_destroy(sesEcho);

	sppPreprocess = speex_preprocess_state_init(iFrameSize, iSampleRate);

	iArg = 1;
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_VAD, &iArg);
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_AGC, &iArg);
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_DENOISE, &iArg);
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_DEREVERB, &iArg);

	iArg = 30000;
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_AGC_TARGET, &iArg);

	float v = 30000.0f / static_cast<float>(g.s.iMinLoudness);
	iArg = iroundf(floorf(20.0f * log10f(v)));
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &iArg);

	iArg = -60;
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_AGC_DECREMENT, &iArg);

	iArg = g.s.iNoiseSuppress;
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &iArg);

	if (iEchoChannels > 0) {
		sesEcho = speex_echo_state_init_mc(iFrameSize, iFrameSize * 10, 1, bEchoMulti ? iEchoChannels : 1);
		iArg = iSampleRate;
		speex_echo_ctl(sesEcho, SPEEX_ECHO_SET_SAMPLING_RATE, &iArg);
		speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_ECHO_STATE, sesEcho);

		qWarning("AudioInput: ECHO CANCELLER ACTIVE");
	} else {
		sesEcho = NULL;
	}

	bResetEncoder = true;

	bResetProcessor = false;
}

bool AudioInput::selectCodec() {
	bool useOpus = false;

	// Currently talking, use previous Opus status.
	if (bPreviousVoice) {
		useOpus = (umtType == MessageHandler::UDPVoiceOpus);
	} else {
#ifdef USE_OPUS
		if (g.bOpus || (g.s.lmLoopMode == Settings::Local)) {
			useOpus = true;
		}
#endif
	}

	if (!useOpus) {
		CELTCodec *switchto = NULL;
		if ((!g.uiSession || (g.s.lmLoopMode == Settings::Local)) && (!g.qmCodecs.isEmpty())) {
			// Use latest for local loopback
			QMap<int, CELTCodec *>::const_iterator i = g.qmCodecs.constEnd();
			--i;
			switchto = i.value();
		} else {
			// Currently talking, don't switch unless you must.
			if (cCodec && bPreviousVoice) {
				int v = cCodec->bitstreamVersion();
				if ((v == g.iCodecAlpha) || (v == g.iCodecBeta))
					switchto = cCodec;
			}
		}
		if (!switchto) {
			switchto = g.qmCodecs.value(g.bPreferAlpha ? g.iCodecAlpha : g.iCodecBeta);
			if (!switchto)
				switchto = g.qmCodecs.value(g.bPreferAlpha ? g.iCodecBeta : g.iCodecAlpha);
		}
		if (switchto != cCodec) {
			if (cCodec && ceEncoder) {
				cCodec->celt_encoder_destroy(ceEncoder);
				ceEncoder = NULL;
			}
			cCodec = switchto;
			if (cCodec)
				ceEncoder = cCodec->encoderCreate();
		}

		if (!cCodec)
			return false;
	}

	MessageHandler::UDPMessageType previousType = umtType;
	if (useOpus) {
		umtType = MessageHandler::UDPVoiceOpus;
	} else {
		if (!g.uiSession) {
			umtType = MessageHandler::UDPVoiceCELTAlpha;
		} else {
			int v = cCodec->bitstreamVersion();
			if (v == g.iCodecAlpha)
				umtType = MessageHandler::UDPVoiceCELTAlpha;
			else if (v == g.iCodecBeta)
				umtType = MessageHandler::UDPVoiceCELTBeta;
			else {
				qWarning() << "Couldn't find message type for codec version" << v;
			}
		}
	}

	if (umtType != previousType) {
		iBufferedFrames = 0;
		qlFrames.clear();
		opusBuffer.clear();
	}

	return true;
}

int AudioInput::encodeOpusFrame(short *source, int size, EncodingOutputBuffer& buffer) {
	int len;
#ifdef USE_OPUS
	if (!oCodec) {
		return 0;
	}

	if (bResetEncoder) {
		oCodec->opus_encoder_ctl(opusState, OPUS_RESET_STATE, NULL);
		bResetEncoder = false;
	}

	oCodec->opus_encoder_ctl(opusState, OPUS_SET_BITRATE(iAudioQuality));

	len = oCodec->opus_encode(opusState, source, size, &buffer[0], static_cast<opus_int32>(buffer.size()));
	const int tenMsFrameCount = (size / iFrameSize);
	iBitrate = (len * 100 * 8) / tenMsFrameCount;
#endif
	return len;
}

int AudioInput::encodeCELTFrame(short *psSource, EncodingOutputBuffer& buffer) {
	int len;
	if (!cCodec)
		return 0;

	if (bResetEncoder) {
		cCodec->celt_encoder_ctl(ceEncoder, CELT_RESET_STATE);
		bResetEncoder = false;
	}

	cCodec->celt_encoder_ctl(ceEncoder, CELT_SET_PREDICTION(0));

	cCodec->celt_encoder_ctl(ceEncoder, CELT_SET_VBR_RATE(iAudioQuality));
	len = cCodec->encode(ceEncoder, psSource, &buffer[0], qMin<int>(iAudioQuality / (8 * 100), static_cast<int>(buffer.size())));
	iBitrate = len * 100 * 8;

	return len;
}

void AudioInput::encodeAudioFrame() {
	int iArg;
	int i;
	float sum;
	short max;

	short *psSource;

	iFrameCounter++;

	// As g.iTarget is not protected by any locks, we avoid race-conditions by
	// copying it once at this point and stick to whatever value it is here. Thus
	// if the value of g.iTarget changes during the execution of this function,
	// it won't cause any inconsistencies and the change is reflected once this
	// function is called again.
	int voiceTargetID = g.iTarget;

	if (! bRunning)
		return;

	sum=1.0f;
	max = 1;
	for (i=0;i<iFrameSize;i++) {
		sum += static_cast<float>(psMic[i] * psMic[i]);
		max = std::max(static_cast<short>(abs(psMic[i])), max);
	}
	dPeakMic = qMax(20.0f*log10f(sqrtf(sum / static_cast<float>(iFrameSize)) / 32768.0f), -96.0f);
	dMaxMic = max;

	if (psSpeaker && (iEchoChannels > 0)) {
		sum=1.0f;
		for (i=0;i<iFrameSize;i++)
			sum += static_cast<float>(psSpeaker[i] * psSpeaker[i]);
		dPeakSpeaker = qMax(20.0f*log10f(sqrtf(sum / static_cast<float>(iFrameSize)) / 32768.0f), -96.0f);
	} else {
		dPeakSpeaker = 0.0;
	}

	QMutexLocker l(&qmSpeex);
	resetAudioProcessor();

#ifdef USE_RNNOISE
	// At the time of writing this code, RNNoise only supports a sample rate of 48000 Hz.
	if (g.s.bDenoise && denoiseState && (iFrameSize == 480)) {
		float denoiseFrames[480];
		for (int i = 0; i < 480; i++) {
			denoiseFrames[i] = psMic[i];
		}

		rnnoise_process_frame(denoiseState, denoiseFrames, denoiseFrames);

		for (int i = 0; i < 480; i++) {
			psMic[i] = denoiseFrames[i];
		}
	}
#endif

	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_GET_AGC_GAIN, &iArg);
	float gainValue = static_cast<float>(iArg);
	iArg = g.s.iNoiseSuppress - iArg;
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &iArg);

	if (sesEcho && psSpeaker) {
		speex_echo_cancellation(sesEcho, psMic, psSpeaker, psClean);
		speex_preprocess_run(sppPreprocess, psClean);
		psSource = psClean;
	} else {
		speex_preprocess_run(sppPreprocess, psMic);
		psSource = psMic;
	}

	sum=1.0f;
	for (i=0;i<iFrameSize;i++)
		sum += static_cast<float>(psSource[i] * psSource[i]);
	float micLevel = sqrtf(sum / static_cast<float>(iFrameSize));
	dPeakSignal = qMax(20.0f*log10f(micLevel / 32768.0f), -96.0f);

	spx_int32_t prob = 0;
	speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_GET_PROB, &prob);
	fSpeechProb = static_cast<float>(prob) / 100.0f;

	// clean microphone level: peak of filtered signal attenuated by AGC gain
	dPeakCleanMic = qMax(dPeakSignal - gainValue, -96.0f);
	float level = (g.s.vsVAD == Settings::SignalToNoise) ? fSpeechProb : (1.0f + dPeakCleanMic / 96.0f);

	bool bIsSpeech = false;

	if (level > g.s.fVADmax) {
		// Voice-activation threshold has been reached
		bIsSpeech = true;
	} else if (level > g.s.fVADmin && bPreviousVoice) {
		// Voice-deactivation threshold has not yet been reached
		bIsSpeech = true;
	}

	if (! bIsSpeech) {
		iHoldFrames++;
		if (iHoldFrames < g.s.iVoiceHold)
			bIsSpeech = true;
	} else {
		iHoldFrames = 0;
	}

	if (g.s.atTransmit == Settings::Continuous) {
		// Continous transmission is enabled
		bIsSpeech = true;
	} else if (g.s.atTransmit == Settings::PushToTalk) {
		// PTT is enabled, so check if it is currently active
		bIsSpeech = g.s.uiDoublePush && ((g.uiDoublePush < g.s.uiDoublePush) || (g.tDoublePush.elapsed() < g.s.uiDoublePush));
	}

	// If g.iPushToTalk > 0 that means that we are currently in some sort of PTT action. For
	// instance this could mean we're currently whispering
	bIsSpeech = bIsSpeech || (g.iPushToTalk > 0);

	ClientUser *p = ClientUser::get(g.uiSession);
	if (g.s.bMute || ((g.s.lmLoopMode != Settings::Local) && p && (p->bMute || p->bSuppress)) || g.bPushToMute || (voiceTargetID < 0)) {
		bIsSpeech = false;
	}

#ifdef USE_EXTPTT
	if(g.extptt) bIsSpeech |= g.extptt->getPTT();
#endif
#ifdef USE_RTPAUDIO
	if(g.ai) bIsSpeech |= g.ai->getPTT();
#endif

	if (bIsSpeech) {
		iSilentFrames = 0;
	} else {
		iSilentFrames++;
		if (iSilentFrames > 500)
			iFrameCounter = 0;
	}

	if (p) {
		if (! bIsSpeech)
			p->setTalking(Settings::Passive);
		else if (voiceTargetID == 0)
			p->setTalking(Settings::Talking);
		else
			p->setTalking(Settings::Shouting);
	}

	if (g.s.bTxAudioCue && g.uiSession != 0) {
		AudioOutputPtr ao = g.ao;
		if (bIsSpeech && ! bPreviousVoice && ao)
			ao->playSample(g.s.qsTxAudioCueOn);
		else if (ao && !bIsSpeech && bPreviousVoice)
			ao->playSample(g.s.qsTxAudioCueOff);
	}

	if (! bIsSpeech && ! bPreviousVoice) {
		iBitrate = 0;

		if ((tIdle.elapsed() / 1000000ULL) > g.s.iIdleTime) {
			activityState = ActivityStateIdle;
			tIdle.restart();
			if (g.s.iaeIdleAction == Settings::Deafen && !g.s.bDeaf) {
				emit doDeaf();
			} else if (g.s.iaeIdleAction == Settings::Mute && !g.s.bMute) {
				emit doMute();
			}
		}

		if (activityState == ActivityStateReturnedFromIdle) {
			activityState = ActivityStateActive;
			if (g.s.iaeIdleAction != Settings::Nothing && g.s.bUndoIdleActionUponActivity) {
				if (g.s.iaeIdleAction == Settings::Deafen && g.s.bDeaf) {
					emit doDeaf();
				} else if (g.s.iaeIdleAction == Settings::Mute && g.s.bMute) {
					emit doMute();
				}
			}
		}

		spx_int32_t increment = 0;
		speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_AGC_INCREMENT, &increment);
		return;
	} else {
		spx_int32_t increment = 12;
		speex_preprocess_ctl(sppPreprocess, SPEEX_PREPROCESS_SET_AGC_INCREMENT, &increment);
	}

	if (bIsSpeech && !bPreviousVoice) {
		bResetEncoder = true;
	}

	tIdle.restart();

	EncodingOutputBuffer buffer;
	Q_ASSERT(buffer.size() >= static_cast<size_t>(iAudioQuality / 100 * iAudioFrames / 8));
	
	int len = 0;

	bool encoded = true;
	if (!selectCodec())
		return;

	if (umtType == MessageHandler::UDPVoiceCELTAlpha || umtType == MessageHandler::UDPVoiceCELTBeta) {
		len = encodeCELTFrame(psSource, buffer);
		if (len <= 0) {
			iBitrate = 0;
			qWarning() << "encodeCELTFrame failed" << iBufferedFrames << iFrameSize << len;
			return;
		}
		++iBufferedFrames;
	} else if (umtType == MessageHandler::UDPVoiceOpus) {
		encoded = false;
		opusBuffer.insert(opusBuffer.end(), psSource, psSource + iFrameSize);
		++iBufferedFrames;

		if (!bIsSpeech || iBufferedFrames >= iAudioFrames) {
			if (iBufferedFrames < iAudioFrames) {
				// Stuff frame to framesize if speech ends and we don't have enough audio
				// this way we are guaranteed to have a valid framecount and won't cause
				// a codec configuration switch by suddenly using a wildly different
				// framecount per packet.
				const int missingFrames = iAudioFrames - iBufferedFrames;
				opusBuffer.insert(opusBuffer.end(), iFrameSize * missingFrames, 0);
				iBufferedFrames += missingFrames;
				iFrameCounter += missingFrames;
			}
			
			Q_ASSERT(iBufferedFrames == iAudioFrames);

			len = encodeOpusFrame(&opusBuffer[0], iBufferedFrames * iFrameSize, buffer);
			opusBuffer.clear();
			if (len <= 0) {
				iBitrate = 0;
				qWarning() << "encodeOpusFrame failed" << iBufferedFrames << iFrameSize << len;
				iBufferedFrames = 0; // These are lost. Make sure not to mess up our sequence counter next flushCheck.
				return;
			}
			encoded = true;
		}
	}

	if (encoded) {
		flushCheck(QByteArray(reinterpret_cast<char *>(&buffer[0]), len), !bIsSpeech, voiceTargetID);
	}

	if (! bIsSpeech)
		iBitrate = 0;

	bPreviousVoice = bIsSpeech;
}

static void sendAudioFrame(const char *data, PacketDataStream &pds) {
	ServerHandlerPtr sh = g.sh;
	if (sh) {
		VoiceRecorderPtr recorder(sh->recorder);
		if (recorder)
			recorder->getRecordUser().addFrame(QByteArray(data, pds.size() + 1));
	}

	if (g.s.lmLoopMode == Settings::Local)
		LoopUser::lpLoopy.addFrame(QByteArray(data, pds.size() + 1));
	else if (sh)
		sh->sendMessage(data, pds.size() + 1);
}

void AudioInput::flushCheck(const QByteArray &frame, bool terminator, int voiceTargetID) {
	qlFrames << frame;

	if (! terminator && iBufferedFrames < iAudioFrames)
		return;

	int flags = 0;
	if (voiceTargetID > 0) {
		flags = voiceTargetID;
	}
	if (terminator && g.iPrevTarget > 0) {
		// If we have been whispering to some target but have just ended, terminator will be true. However
		// in the case of whispering this means that we just released the whisper key so this here is the
		// last audio frame that is sent for whispering. The whisper key being released means that g.iTarget
		// is reset to 0 by now. In order to send the last whisper frame correctly, we have to use
		// g.iPrevTarget which is set to whatever g.iTarget has been before its last change.

		flags = g.iPrevTarget;

		// We reset g.iPrevTarget as it has fulfilled its purpose for this whisper-action. It'll be set
		// accordingly once the client whispers for the next time.
		g.iPrevTarget = 0;
	}

	if (g.s.lmLoopMode == Settings::Server)
		flags = 0x1f; // Server loopback

	flags |= (umtType << 5);

	char data[1024];
	data[0] = static_cast<unsigned char>(flags);

	int frames = iBufferedFrames;
	iBufferedFrames = 0;

	PacketDataStream pds(data + 1, 1023);
	// Sequence number
	pds << iFrameCounter - frames;

	if (umtType == MessageHandler::UDPVoiceOpus) {
		const QByteArray &qba = qlFrames.takeFirst();
		int size = qba.size();
		if (terminator)
			size |= 1 << 13;
		pds << size;
		pds.append(qba.constData(), qba.size());
	} else {
		if (terminator) {
			qlFrames << QByteArray();
			++frames;
		}

		for (int i = 0; i < frames; ++i) {
			const QByteArray &qba = qlFrames.takeFirst();
			unsigned char head = static_cast<unsigned char>(qba.size());
			if (i < frames - 1)
				head |= 0x80;
			pds.append(head);
			pds.append(qba.constData(), qba.size());
		}
	}

	if (g.s.bTransmitPosition && g.p && ! g.bCenterPosition && g.p->fetch()) {
		pds << g.p->fPosition[0];
		pds << g.p->fPosition[1];
		pds << g.p->fPosition[2];
	}

	sendAudioFrame(data, pds);

	Q_ASSERT(qlFrames.isEmpty());
}

bool AudioInput::isAlive() const {
	return isRunning();
}
