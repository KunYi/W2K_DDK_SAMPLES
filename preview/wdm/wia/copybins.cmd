rem  Copy all binaries to an installable location. An optional parameter
rem  is the destination directory which defaults to the subdirectory "wiabins".

md %1wiabins
md %1wiabins\microdrv
md %1wiabins\scanner
md %1wiabins\camera

copy microdrv\obj%build_alt_dir%\i386\testmcro.dll %1wiabins\microdrv
copy microdrv\testmcro.inf %1wiabins\microdrv

copy wiatscan\obj%build_alt_dir%\i386\wiatscan.dll %1wiabins\scanner
copy wiatscan\wiatscan.inf %1wiabins\scanner
copy wiatscan\*.bmp %1wiabins\scanner

copy testcam\obj%build_alt_dir%\i386\testcam.dll %1wiabins\camera
copy testcam\testcam.inf %1wiabins\camera
copy extend\obj%build_alt_dir%\i386\extend.dll %1wiabins\camera
copy extend\tcamlogo.jpg %1wiabins\camera
copy extend\testcam.ico %1wiabins\camera

copy getimage\obj%build_alt_dir%\i386\getimage.exe %1wiabins

copy wiatest\wiatest.exe %1wiabins
copy tools\wialogcfg.exe %1wiabins



