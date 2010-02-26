SUBDIRS = . src

clean-local:
	-rm -rf *~ *.bak

distclean-local:
	-rm -rf autom4te.cache

log:
	svn update
	svn log > ChangeLog
