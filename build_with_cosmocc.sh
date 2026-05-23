#!/bin/bash

(
	rm -rf cosmocc
	mkdir cosmocc
	cd cosmocc
	wget https://cosmo.zip/pub/cosmocc/cosmocc.zip
	unzip cosmocc.zip
)

PATH="$(pwd)/cosmocc/bin:$PATH" make CC=cosmocc clean
PATH="$(pwd)/cosmocc/bin:$PATH" make CC="cosmocc -Wno-implicit-function-declaration"
