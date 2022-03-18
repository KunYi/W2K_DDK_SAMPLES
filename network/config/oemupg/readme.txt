1. The files sources & makefile are useful only if you are going to build
this within NT build environment.

2. oemupgex.h is a part of the DDK. It is included here just in case you
do not have the latest DDK.

3. use sources.ddk and pch.h.ddk instead of source & pch.h if you want
   to build this in DDK build environment.
