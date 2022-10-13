# HG changeset patch
# User Konstantin Pavlov <thresh@nginx.com>
# Date 1666010753 -14400
#      Mon Oct 17 16:45:53 2022 +0400
# Node ID 91283adcafa109108af9e40d60f611c028d971b2
# Parent  0b87c0309b37a99f1ae263faa7f5f82177454364
Added a target to generate pkg-config file.

diff -r 0b87c0309b37 -r 91283adcafa1 auto/make
--- a/auto/make	Mon Oct 10 19:01:56 2022 -0700
+++ b/auto/make	Mon Oct 17 16:45:53 2022 +0400
@@ -64,7 +64,7 @@ default: njs
 NJS_LIB_INCS = $njs_incs
 NJS_LIB_OBJS = $njs_objs
 
-libnjs: $NJS_BUILD_DIR/libnjs.a
+libnjs: $NJS_BUILD_DIR/libnjs.a pc
 
 $NJS_BUILD_DIR/libnjs.a:   \\
 	$NJS_BUILD_DIR/njs_auto_config.h \\
@@ -314,6 +314,19 @@ ts_clean:
 	rm -rf $NJS_BUILD_DIR/ts
 END
 
+# pkg-config file
+cat << END >> $NJS_MAKEFILE
+
+pc: $NJS_BUILD_DIR/njs.pc
+
+$NJS_BUILD_DIR/njs.pc: $NJS_BUILD_DIR/njs_auto_config.h
+	sed -e "s,@PREFIX@,$(pwd)/$NJS_BUILD_DIR," \\
+		-e "s,@LIBDIR@,$(pwd)/$NJS_BUILD_DIR," \\
+		-e "s,@CFLAGS@,-I$(pwd)/$NJS_BUILD_DIR -I$(pwd)/$NJS_BUILD_DIR/../src," \\
+		-e "s,@VERSION@,\$(NJS_VER)," \\
+		-e "s,@EXTRA_LIBS@,-lm $NJS_LIBS $NJS_LIB_AUX_LIBS," \\
+		src/njs.pc.in > \$@
+END
 
 # Makefile.
 
diff -r 0b87c0309b37 -r 91283adcafa1 src/njs.pc.in
--- /dev/null	Thu Jan 01 00:00:00 1970 +0000
+++ b/src/njs.pc.in	Mon Oct 17 16:45:53 2022 +0400
@@ -0,0 +1,8 @@
+prefix=@PREFIX@
+libdir=@LIBDIR@
+
+Name: njs
+Description: library to embed njs scripting language
+Version: @VERSION@
+Libs: -L${libdir} -lnjs @EXTRA_LIBS@
+Cflags: @CFLAGS@
