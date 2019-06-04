# the absolute path to the extracted boost source archive (https://www.boost.org/users/download/)
set -e
set -x
BOOSTPATH=/tmp/boost_1_69_0
TMPPATH=/tmp/lslboost

# copy all needed boost files and rename all mentions of boost to lslboost
mkdir -p $TMPPATH
bcp --unix-lines --boost=$BOOSTPATH --namespace=lslboost --scan `find src -regex ".+\.[ch]p*"` lslboost/asio_objects.cpp $TMPPATH
# remove superfluous directories:
rm $TMPPATH/Jamroot
find $TMPPATH -type d -and \( -name build -o -name test -o -name edg -o -name dmc -o -name msvc70 -o -name msvc60 -o -name bcc* \) -print0 | xargs -0 rm -rf

rsync -HAXavr --del $TMPPATH/{boost,libs} lslboost

# apply patches to boost
# remove minimum wait time
patch -p3 < lslboost/boost-thread-windows.patch
find lslboost -name '*.orig' -delete
