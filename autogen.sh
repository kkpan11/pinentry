#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# It is only needed for the CVS version.

PGM=GPINENTRY
lib_config_files=""
autoconf_vers=2.52
automake_vers=1.5
aclocal_vers=1.5
#libtool_vers=1.3

DIE=no
if (autoconf --version) < /dev/null > /dev/null 2>&1 ; then
    if (autoconf --version | awk 'NR==1 { if( $3 >= '$autoconf_vers') \
			       exit 1; exit 0; }');
    then
       echo "**Error**: "\`autoconf\'" is too old."
       echo '           (version ' $autoconf_vers ' or newer is required)'
       DIE="yes"
    fi
else
    echo
    echo "**Error**: You must have "\`autoconf\'" installed to compile $PGM."
    echo '           (version ' $autoconf_vers ' or newer is required)'
    DIE="yes"
fi

if (automake --version) < /dev/null > /dev/null 2>&1 ; then
  if (automake --version | awk 'NR==1 { if( $4 >= '$automake_vers') \
			     exit 1; exit 0; }');
     then
     echo "**Error**: "\`automake\'" is too old."
     echo '           (version ' $automake_vers ' or newer is required)'
     DIE="yes"
  fi
  if (aclocal --version) < /dev/null > /dev/null 2>&1; then
    if (aclocal --version | awk 'NR==1 { if( $4 >= '$aclocal_vers' ) \
						exit 1; exit 0; }' );
    then
      echo "**Error**: "\`aclocal\'" is too old."
      echo '           (version ' $aclocal_vers ' or newer is required)'
      DIE="yes"
    fi
  else
    echo
    echo "**Error**: Missing "\`aclocal\'".  The version of "\`automake\'
    echo "           installed doesn't appear recent enough."
    DIE="yes"
  fi
else
    echo
    echo "**Error**: You must have "\`automake\'" installed to compile $PGM."
    echo '           (version ' $automake_vers ' or newer is required)'
    DIE="yes"
fi


#if (libtool --version) < /dev/null > /dev/null 2>&1 ; then
#    if (libtool --version | awk 'NR==1 { if( $4 >= '$libtool_vers') \
#                               exit 1; exit 0; }');
#    then
#       echo "**Error**: "\`libtool\'" is too old."
#       echo '           (version ' $libtool_vers ' or newer is required)'
#       DIE="yes"
#    fi
#else
#    echo
#    echo "**Error**: You must have "\`libtool\'" installed to compile $PGM."
#    echo '           (version ' $libtool_vers ' or newer is required)'
#    DIE="yes"
#fi
#

if [ ! -f assuan/assuan.h ]; then
    echo "**Error**: You must must have a copy of the assuan source in"
    echo "           the assuan/ directory.  Assuan is maintained as part"
    echo "           of the NewPG Module in the Aegyptyen CVS."
    DIE="yes"
fi

if test "$DIE" = "yes"; then
    exit 1
fi

#echo "Running libtoolize...  Ignore non-fatal messages."
#echo "no" | libtoolize

echo "Running aclocal..."
aclocal
echo "Running autoheader..."
autoheader
echo "Running automake --gnu -a ..."
automake --gnu -a
echo "Running autoconf..."
autoconf
