# Copyright (C) 2012 The SBCELT Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE-file.

include Make.conf

.PHONY: default
default:
	$(MAKE) -C lib
	$(MAKE) -C helper
	$(MAKE) -C bench-celt
	$(MAKE) -C bench-sbcelt

.PHONY: clean
clean:
	$(MAKE) -C lib clean
	$(MAKE) -C helper clean
	$(MAKE) -C bench-celt clean
	$(MAKE) -C bench-sbcelt clean
	@cd test && ./clean.bash

.PHONY: test
test:
	@cd test && ./run.bash
	
