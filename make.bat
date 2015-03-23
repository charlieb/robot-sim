REM c:\Localdata\gtk2.24-32\bin\pkg-config.exe --cflags --libs gtk+-2.0

set CFLAGS=-m32 -mms-bitfields -Ic:/Localdata/gtk2.24-32/include/gtk-2.0 -Ic:/Localdata/gtk2.24-32/lib/gtk-2.0/include -Ic:/Localdata/gtk2.24-32/include/atk-1.0 -Ic:/Localdata/gtk2.24-32/include/cairo -Ic:/Localdata/gtk2.24-32/include/gdk-pixbuf-2.0 -Ic:/Localdata/gtk2.24-32/include/pango-1.0 -Ic:/Localdata/gtk2.24-32/include/glib-2.0 -Ic:/Localdata/gtk2.24-32/lib/glib-2.0/include -Ic:/Localdata/gtk2.24-32/include/pixman-1 -Ic:/Localdata/gtk2.24-32/include -Ic:/Localdata/gtk2.24-32/include/freetype2 -Ic:/Localdata/gtk2.24-32/include/libpng14  

set LIBS=-Lc:/Localdata/gtk2.24-32/lib -lgtk-win32-2.0 -lgdk-win32-2.0 -lcairo -lgobject-2.0 -lglib-2.0

PATH=c:/localdata/mingw/lib;c:/localdata/mingw/bin;c:/localdata/gtk2.24-32/bin;c:/localdata/gtk2.24-32/lib;%PATH%

C:/localdata/mingw/bin/gcc.exe -std=c99 %CFLAGS% *.c -o robot_sim.exe %LIBS%
rem C:/localdata/mingw/bin/ld.exe -o robot_sim.exe robot_sim.o %LIBS%

