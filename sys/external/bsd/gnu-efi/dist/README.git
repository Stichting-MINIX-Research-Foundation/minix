README.git

Generating releases from git a very simple process;

1) Edit the file "Makefile".  Changing the "VERSION" line to the new version.
2) Do a "git commit" just for the version number change.
3) Then do a "make test-archive".
That will make a file in the current directory gnu-efi-$VERSION.tar.bz2 ,
with its top level directory gnu-efi-$VERSION/ and the source tree under that.
    
Once you've tested that and you're sure it's what you want to release,
4) Do "make archive", which will tag a release in git and generate a
final tarball from it.

You then push to the archive, being sure to include the tag:
5) "git push origin master:master --tags"

And upload the archive wherever it's supposed to go.
    


