VSINSTALLDIR="L:\flash2G_my\syser\pdb\tmp"
#CFLAGS= /MT
LFLAGS= /MANIFEST:NO
#DEBUG= NO

!if "$(DEBUG)" == ""
DEBUG = 1
!endif

!if "$(DEBUG)" == "1"
ODIR = debug
CFLAGS = $(CFLAGS) -Od -D_DEBUG -ZI 
D = d
!else
ODIR = release
CFLAGS = $(CFLAGS) -O1 -DNDEBUG -Zi
!endif


LIBS = \
    $(LIBS) \
    ole32.lib \
    oleaut32.lib


#CFLAGS = $(CFLAGS) -MD$(D) -I"$(VSINSTALLDIR)\DIA SDK 2013\include"
CFLAGS = $(CFLAGS) -MT$(D) -I"$(VSINSTALLDIR)\DIA SDK 2014\include"

#PCHNAME  = $(ODIR)\stdafx.pch
#PCHHEADER = stdafx.h
#PCHFLAGS = -Yu$(PCHHEADER) -Fp$(PCHNAME)
CFLAGS   = $(CFLAGS)   -nologo -c -Fd$(ODIR)\ -W3
LFLAGS   = $(LFLAGS)   -map -debug -PDB:$(ODIR)\dia2dump.pdb "-libpath:$(VSINSTALLDIR)\DIA SDK 2014\lib"


!if "$(VERBOSE)" == "1"
!message DEBUG=$(DEBUG)
!endif

OBJS = \
    $(ODIR)\main.obj    \
    $(ODIR)\PDB.obj        \
    $(ODIR)\PDBExtractor.obj \
    $(ODIR)\PDBHeaderReconstructor.obj


##### Inference Rules

all : $(ODIR)\dia2dump.exe


{}.cpp{$(ODIR)\}.obj::
    cl $(CFLAGS) /std:c++latest $(MPBUILDFLAGS) $(PCHFLAGS) -Fo$(ODIR)\ -FR$(ODIR)\ $<

$(ODIR)\dia2dump.exe : $(ODIR) $(PCHNAME) $(OBJS)
    link -out:$(ODIR)\dia2dump.exe $(OBJS) $(LFLAGS) $(LIBS)

$(ODIR):
    -md $(ODIR)

clean :
    del /q $(ODIR)
