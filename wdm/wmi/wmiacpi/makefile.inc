#
# generate auxilary files from the binary mof (bmf) file
#
# acpimof.vbs is a ready to run vbscript applet that will query all classes
#            in the mof. Once you add the ASL to the bios, 
#            update the registry, reboot, and ensure that the wmiacpi driver
#            has loaded run the script at the command line by typing: 
#            acpimof.vbs.

clean:
    del acpimof.vbs

acpimof.vbs: acpimof.bmf
    wmimofck -tacpimof.vbs acpimof.bmf

