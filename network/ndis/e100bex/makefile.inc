clean: 
    del e100bdat.h e100b.bmf

e100bdat.h: e100b.bmf
    wmimofck -he100bdat.h e100b.bmf

