EMACS=/local/src/emacs-20.3/src/emacs

all: vtx.elc 

vtx.elc: vtx.el
	$(EMACS) -batch --eval '(byte-compile-file "vtx.el")'

clean:
	rm -f *.elc *~

distclean:
	make clean
	rm -f *~ #*#

install:
	cp *.elc $(INSTALLATION_DIRECTORY)

