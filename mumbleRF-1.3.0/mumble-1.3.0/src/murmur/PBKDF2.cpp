// Copyright 2005-2019 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

/* Copyright (C) 2013, Morris Moraes <mauricioarozi@gmail.com>
   Copyright (C) 2014, Stefan Hacker <dd0t@users.sourceforge.net>

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

#include "murmur_pch.h"

#include "PBKDF2.h"

int PBKDF2::benchmark() {
	const QString pseudopass(QLatin1String("aboutAvg"));
	const QString hexSalt = getSalt(); // Could tolerate not getting a salt here, will likely only make it harder.
	
	int maxIterations = -1;
	
	QElapsedTimer timer;
	timer.start();
	
	for (size_t i = 0; i < BENCHMARK_N; ++i) {
		int iterations = BENCHMARK_MINIMUM_ITERATION_COUNT / 2;
	
		timer.restart();
		do {
			iterations *= 2;
			
			// Store return value in a volatile to prevent optimizer
			// from ever removing these side-effect-free calls. I don't
			// think the compiler can prove they have no side-effects but
			// better safe than sorry.
			volatile QString result = getHash(hexSalt, pseudopass, iterations);
			Q_UNUSED(result);
			
		} while (timer.restart() < BENCHMARK_DURATION_TARGET_IN_MS && (iterations / 2) < std::numeric_limits<int>::max());
		
		if (iterations > maxIterations) {
			maxIterations = iterations;
		}
	}
	return maxIterations;
}

QString PBKDF2::getHash(const QString &hexSalt, const QString &password, int iterationCount) {
	QByteArray hash(DERIVED_KEY_LENGTH, 0);
	
	const QByteArray utf8Password = password.toUtf8();
	const QByteArray salt = QByteArray::fromHex(hexSalt.toLatin1());

	if (PKCS5_PBKDF2_HMAC(utf8Password.constData(), utf8Password.size(),
	                      reinterpret_cast<const unsigned char*>(salt.constData()), salt.size(),
	                      iterationCount,
	                      EVP_sha384(),
	                      DERIVED_KEY_LENGTH, reinterpret_cast<unsigned char*>(hash.data())) == 0) {
		qFatal("PBKDF2: PKCS5_PBKDF2_HMAC failed: %s", ERR_error_string(ERR_get_error(), NULL));
		return QString();
	}
	
	return hash.toHex();
}


QString PBKDF2::getSalt() {
	QByteArray salt(SALT_LENGTH, 0);
	
	if (RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), salt.size()) != 1) {
		qFatal("PBKDF2: RAND_bytes for salt failed: %s", ERR_error_string(ERR_get_error(), NULL));
		return QString();
	}

	return salt.toHex();
}
